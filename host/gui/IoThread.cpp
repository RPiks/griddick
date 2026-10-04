/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * IoThread.cpp - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "IoThread.h"

#include "gt.h"

#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

static QString scrapeVersion(const char *line)
{
    const char *p = std::strstr(line, "\"version\"");
    const char *e;

    if (p == nullptr) {
        return {};
    }
    p = std::strchr(p + 9, '"');
    if (p == nullptr) {
        return {};
    }
    p++;
    e = std::strchr(p, '"');
    if (e == nullptr || e == p || (e - p) > 24) {
        return {};
    }
    return QString::fromLatin1(p, static_cast<int>(e - p));
}

static bool scrapeRms(const char *line, unsigned *out)
{
    const char *p;

    if (std::strstr(line, "[Runtime]") == nullptr) {
        return false;
    }
    p = std::strstr(line, "RMS:");
    if (p == nullptr) {
        return false;
    }
    *out = static_cast<unsigned>(std::strtoul(p + 4, nullptr, 10));
    return true;
}

IoThread::IoThread(QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_not(nullptr)
    , m_pendTimer(nullptr)
    , m_injTimer(nullptr)
    , m_pend(PendNone)
    , m_gotCfg(false)
    , m_timeReport(false)
    , m_lab(LabOff)
    , m_wavFd(-1)
    , m_wavLeft(0)
    , m_injSeq(0)
    , m_ax25(0)
    , m_fcsOk(0)
    , m_fcsFail(0)
    , m_callOn(false)
    , m_callListen(false)
    , m_callFlight(0)
    , m_callTimer(nullptr)
{
    kiss_rx_init(&m_rx);
    std::memset(&m_callOpts, 0, sizeof(m_callOpts));
    std::memset(&m_call, 0, sizeof(m_call));
    m_pendTimer = new QTimer(this);
    m_pendTimer->setSingleShot(true);
    connect(m_pendTimer, &QTimer::timeout, this, &IoThread::onPendTimeout);
    m_injTimer = new QTimer(this);
    connect(m_injTimer, &QTimer::timeout, this, &IoThread::onInjectTick);
    m_callTimer = new QTimer(this);
    connect(m_callTimer, &QTimer::timeout, this, &IoThread::onCallTick);
}

IoThread::~IoThread()
{
    closeFd();
}

void IoThread::endLab()
{
    if (m_injTimer != nullptr) {
        m_injTimer->stop();
    }
    if (m_wavFd >= 0) {
        close(m_wavFd);
        m_wavFd = -1;
    }
    m_wavLeft = 0;
    if (m_lab == LabDump && m_fd >= 0) {
        uint8_t off = KISS_HW_DUMP_OFF;
        (void)writeHw(&off, 1);
        emit dumpActive(false);
    } else if (m_lab == LabInject && m_fd >= 0) {
        uint8_t off = KISS_HW_INJECT_OFF;
        (void)writeHw(&off, 1);
        emit injectActive(false);
        emit injectStats(m_ax25, m_fcsOk, m_fcsFail);
    }
    m_lab = LabOff;
}

void IoThread::closeFd()
{
    if (m_pendTimer != nullptr) {
        m_pendTimer->stop();
    }
    m_pend = PendNone;
    m_gotCfg = false;
    m_timeReport = false;
    disarmCall();
    endLab();
    delete m_not;
    m_not = nullptr;
    if (m_fd >= 0) {
        gt_tty_close(m_fd);
        m_fd = -1;
    }
    kiss_rx_init(&m_rx);
}

void IoThread::armPend(Pend p, int ms)
{
    m_pend = p;
    m_pendTimer->start(ms);
}

void IoThread::startSession(const QString &path)
{
    const QByteArray p = path.toLocal8Bit();
    int fl;

    if (m_fd >= 0) {
        emit status(QStringLiteral("Already connected."));
        return;
    }
    m_fd = gt_tty_open(p.constData());
    if (m_fd < 0) {
        emit status(QStringLiteral("Open failed: %1").arg(
            QString::fromLocal8Bit(std::strerror(errno))));
        emit disconnected();
        return;
    }
    fl = fcntl(m_fd, F_GETFL, 0);
    if (fl >= 0) {
        (void)fcntl(m_fd, F_SETFL, fl | O_NONBLOCK);
    }
    kiss_rx_init(&m_rx);
    m_gotCfg = false;
    m_not = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_not, &QSocketNotifier::activated, this, &IoThread::onReady);

    emit status(QStringLiteral("Opened %1").arg(path));
    /* Time on connect is best-effort; do not steal the status line. */
    if (!writeTime(true, false) || !writeCfgQuery()) {
        emit status(QStringLiteral("Write failed: %1").arg(
            QString::fromLocal8Bit(std::strerror(errno))));
        closeFd();
        emit disconnected();
        return;
    }
}

void IoThread::stopSession()
{
    if (m_fd < 0) {
        emit disconnected();
        return;
    }
    closeFd();
    emit status(QStringLiteral("Device disconnected."));
    emit disconnected();
}

void IoThread::requestCfgGet()
{
    if (m_fd < 0) {
        return;
    }
    if (!writeCfgQuery()) {
        emit status(QStringLiteral("Cfg query write failed."));
    }
}

void IoThread::requestCfgPut(const QString &json)
{
    if (m_fd < 0) {
        return;
    }
    if (!writeCfgPut(json)) {
        emit status(QStringLiteral("Cfg put failed."));
    }
}

void IoThread::requestTimeSync()
{
    if (m_fd < 0) {
        return;
    }
    if (!writeTime(true, true)) {
        emit status(QStringLiteral("Time sync write failed."));
    }
}

void IoThread::requestTimeGet()
{
    if (m_fd < 0) {
        return;
    }
    if (!writeTime(false, true)) {
        emit status(QStringLiteral("Time get write failed."));
    }
}

bool IoThread::writeHw(const uint8_t *pay, int n)
{
    if (m_fd < 0) {
        return false;
    }
    if (gt_kiss_write(m_fd, KISS_CMD_SETHW, pay, n) != 0) {
        return false;
    }
    (void)gt_kiss_drain(m_fd);
    return true;
}

bool IoThread::writeLab(uint8_t hw)
{
    return writeHw(&hw, 1);
}

bool IoThread::writeCfgQuery()
{
    uint8_t hw = KISS_HW_CFG;

    if (!writeHw(&hw, 1)) {
        return false;
    }
    armPend(PendCfg, 1500);
    return true;
}

bool IoThread::writeCfgPut(const QString &json)
{
    QByteArray utf = json.toUtf8();
    char body[KISS_MAX_FRAME];
    uint8_t pay[KISS_MAX_FRAME];
    struct gt_cfg parsed;
    int off = 0;
    int jn;
    int wrapped;

    if (gt_cfg_from_json(utf.constData(), &parsed) != 0) {
        emit status(QStringLiteral("Bad settings JSON."));
        return false;
    }
    jn = kiss_json_span(utf.constData(), utf.size(), &off);
    if (jn < 2 || jn + 6 >= static_cast<int>(sizeof(body))) {
        emit status(QStringLiteral("Bad settings JSON."));
        return false;
    }
    std::memcpy(body, utf.constData() + off, static_cast<size_t>(jn));
    wrapped = kiss_cfg_wrap(body, jn, static_cast<int>(sizeof(body)));
    if (wrapped < 0 || 1 + wrapped > static_cast<int>(sizeof(pay))) {
        return false;
    }
    pay[0] = KISS_HW_CFG;
    std::memcpy(pay + 1, body, static_cast<size_t>(wrapped));
    if (!writeHw(pay, 1 + wrapped)) {
        return false;
    }
    armPend(PendCfg, 1500);
    return true;
}

bool IoThread::writeTime(bool sync, bool reportFail)
{
    uint8_t payload[9];
    uint64_t now;
    int i;

    payload[0] = KISS_HW_TIME;
    if (sync) {
        now = gt_utc_ms();
        for (i = 0; i < 8; i++) {
            payload[1 + i] = static_cast<uint8_t>(now >> (8 * i));
        }
        if (!writeHw(payload, 9)) {
            return false;
        }
    } else if (!writeHw(payload, 1)) {
        return false;
    }
    m_timeReport = reportFail;
    if (reportFail) {
        armPend(PendTime, 2000);
    }
    return true;
}

void IoThread::startDump()
{
    if (m_fd < 0) {
        emit status(QStringLiteral("Not connected."));
        return;
    }
    if (callBusy()) {
        emit status(QStringLiteral("Stop Call first."));
        return;
    }
    if (m_lab == LabDump) {
        return;
    }
    if (m_lab == LabInject) {
        endLab();
    }
    if (!writeLab(KISS_HW_DUMP_ON)) {
        emit status(QStringLiteral("DUMP_ON write failed."));
        return;
    }
    m_lab = LabDump;
    emit dumpActive(true);
    emit status(QStringLiteral("Dump on."));
}

void IoThread::stopDump()
{
    if (m_lab != LabDump) {
        return;
    }
    endLab();
    emit status(QStringLiteral("Dump off."));
}

void IoThread::startInject(const QString &wavPath)
{
    uint32_t ns = 0;
    const QByteArray p = wavPath.toLocal8Bit();

    if (m_fd < 0) {
        emit status(QStringLiteral("Not connected."));
        return;
    }
    if (callBusy()) {
        emit status(QStringLiteral("Stop Call first."));
        return;
    }
    if (m_lab != LabOff) {
        endLab();
    }
    if (gt_wav_open_48k_mono(p.constData(), &m_wavFd, &ns) != 0) {
        emit status(QStringLiteral("Need 48 kHz mono PCM16 WAV."));
        m_wavFd = -1;
        return;
    }
    m_wavLeft = ns;
    m_injSeq = 0;
    m_ax25 = 0;
    m_fcsOk = 0;
    m_fcsFail = 0;
    if (!writeLab(KISS_HW_INJECT_ON)) {
        emit status(QStringLiteral("INJECT_ON write failed."));
        close(m_wavFd);
        m_wavFd = -1;
        return;
    }
    m_lab = LabInject;
    emit injectActive(true);
    emit injectStats(0, 0, 0);
    emit status(QStringLiteral("Inject on."));
    m_injTimer->start(5);
}

void IoThread::stopInject()
{
    if (m_lab != LabInject) {
        return;
    }
    endLab();
    emit status(QStringLiteral("Inject off."));
}

void IoThread::sendKissData(const QByteArray &frame)
{
    if (m_fd < 0) {
        emit status(QStringLiteral("Not connected."));
        return;
    }
    if (m_lab != LabOff) {
        emit status(QStringLiteral("Stop dump/inject first."));
        return;
    }
    if (callBusy()) {
        emit status(QStringLiteral("Stop Call first."));
        return;
    }
    if (frame.size() < 16) {
        emit status(QStringLiteral("Frame too short."));
        return;
    }
    if (gt_kiss_write(m_fd, KISS_CMD_DATA,
                      reinterpret_cast<const uint8_t *>(frame.constData()),
                      frame.size()) != 0) {
        emit status(QStringLiteral("TX write failed."));
        return;
    }
    (void)gt_kiss_drain(m_fd);
    emit status(QStringLiteral("TX %1 bytes.").arg(frame.size()));
}

void IoThread::onInjectTick()
{
    int16_t smp[KISS_PCM_CHUNK];
    int ns;
    ssize_t nb;

    if (m_lab != LabInject || m_wavFd < 0) {
        m_injTimer->stop();
        return;
    }
    if (m_wavLeft == 0) {
        endLab();
        emit status(QStringLiteral("Inject done."));
        return;
    }
    ns = (m_wavLeft > KISS_PCM_CHUNK) ? KISS_PCM_CHUNK : static_cast<int>(m_wavLeft);
    nb = ::read(m_wavFd, smp, static_cast<size_t>(ns) * 2u);
    if (nb != static_cast<ssize_t>(ns) * 2) {
        emit status(QStringLiteral("WAV short read."));
        endLab();
        return;
    }
    if (gt_pcm_write(m_fd, m_injSeq, smp, ns) != 0) {
        emit status(QStringLiteral("PCM write failed."));
        endLab();
        return;
    }
    m_injSeq++;
    m_wavLeft -= static_cast<uint32_t>(ns);
}

void IoThread::onReady()
{
    unsigned char buf[4096];
    ssize_t n;
    int off;

    if (m_fd < 0) {
        return;
    }
    n = ::read(m_fd, buf, sizeof(buf));
    if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            return;
        }
        emit status(QStringLiteral("Read error: %1").arg(
            QString::fromLocal8Bit(std::strerror(errno))));
        closeFd();
        emit disconnected();
        return;
    }
    if (n == 0) {
        emit status(QStringLiteral("Device closed."));
        closeFd();
        emit disconnected();
        return;
    }
    off = 0;
    while (off < n) {
        int used = 0;

        if (gt_kiss_feed(&m_rx, buf + off, static_cast<int>(n) - off, &used)) {
            handleFrame();
            kiss_rx_init(&m_rx);
        }
        if (used <= 0) {
            break;
        }
        off += used;
    }
}

void IoThread::handleFrame()
{
    const uint8_t cmd = m_rx.buf[0] & KISS_CMD_MASK;
    char line[KISS_MAX_FRAME];
    uint8_t seq;
    const uint8_t *samples;
    int ns;
    int crc_ok;

    if (cmd == KISS_CMD_LOG &&
        gt_kiss_log_line(&m_rx, line, static_cast<int>(sizeof(line)))) {
        handleLog(line);
        return;
    }
    if (m_lab == LabDump &&
        gt_pcm_parse(m_rx.buf, m_rx.frame_len, &seq, &samples, &ns, &crc_ok)) {
        if (crc_ok && ns > 0) {
            emit pcmChunk(QByteArray(reinterpret_cast<const char *>(samples),
                                     ns * 2));
        }
        return;
    }
    if (cmd == KISS_CMD_DATA && m_rx.frame_len > 1) {
        char ts[40];
        char out[512];

        gt_stamp(ts, sizeof(ts));
        if (gt_ax25_format_data(m_rx.buf + 1, m_rx.frame_len - 1, 0, ts, out,
                                static_cast<int>(sizeof(out))) >= 0) {
            emit heardFrame(QString::fromLatin1(out));
            if (m_lab == LabInject) {
                emit analysisLine(QString::fromLatin1(out));
            }
        }
        {
            struct gt_ax25_addr dst;
            struct gt_ax25_addr src;
            uint8_t ctrl = 0;
            uint8_t pid = 0;
            int has_pid = 0;
            const uint8_t *info = nullptr;
            int ilen = 0;
            char src_s[16];
            char desc[256];

            if (gt_ax25_parse_frame(m_rx.buf + 1, m_rx.frame_len - 1, &dst, &src,
                                    &ctrl, &pid, &has_pid, &info, &ilen) == 0 &&
                (ctrl & 0xefu) == 0x03u && info != nullptr &&
                gt_aprs_format_rx(info, ilen, desc,
                                  static_cast<int>(sizeof(desc))) > 0) {
                gt_ax25_fmt_call(src_s, sizeof(src_s), &src);
                emit aprsHeard(QStringLiteral("%1 %2 %3")
                                   .arg(QString::fromLatin1(ts),
                                        QString::fromLatin1(src_s),
                                        QString::fromLatin1(desc)));
            }
        }
        if (m_callOn) {
            struct gt_link_ev ev;
            int r = gt_link_feed(&m_call, m_rx.buf + 1, m_rx.frame_len - 1, &ev);

            if (r < 0) {
                emit callError(QStringLiteral("Call i/o error."));
                disarmCall();
            } else {
                emitCallEv(&ev);
            }
        }
        return;
    }
}

void IoThread::handleLog(const char *line)
{
    struct gt_cfg cfg;
    uint64_t utc = 0;
    unsigned rv = 0;
    QString ver;
    int off = 0;
    int jn;

    emit logLine(QString::fromLatin1(line));

    if (std::strstr(line, "[Dump] abort") != nullptr) {
        emit status(QStringLiteral("Dump abort (USB overrun)."));
        if (m_lab == LabDump) {
            m_lab = LabOff;
            emit dumpActive(false);
        }
    }
    if (std::strstr(line, "[Inject] abort") != nullptr) {
        emit status(QStringLiteral("Inject abort."));
        if (m_lab == LabInject) {
            endLab();
        }
    }
    if (m_lab == LabInject && std::strstr(line, "[Ax25]") != nullptr) {
        unsigned alen = 0;
        int fcs = 0;
        const char *p = std::strstr(line, "len=");

        if (p != nullptr) {
            std::sscanf(p, "len=%u fcs=%d", &alen, &fcs);
        }
        m_ax25++;
        if (fcs) {
            m_fcsOk++;
        } else {
            m_fcsFail++;
        }
        emit injectStats(m_ax25, m_fcsOk, m_fcsFail);
        emit analysisLine(QStringLiteral("AX.25 len=%1 fcs=%2").arg(alen).arg(fcs));
    }
    if (scrapeRms(line, &rv)) {
        emit rms(rv);
    }
    if (gt_time_parse_log(line, &utc)) {
        if (m_pend == PendTime) {
            m_pendTimer->stop();
            m_pend = PendNone;
        }
        m_timeReport = false;
        emit timeAck(gt_utc_ms(), utc);
    }
    if (gt_cfg_parse_log(line, &cfg)) {
        if (m_pend == PendCfg) {
            m_pendTimer->stop();
            m_pend = PendNone;
        }
        jn = kiss_json_span(line, static_cast<int>(std::strlen(line)), &off);
        if (jn >= 2) {
            emit cfgJson(QString::fromLatin1(line + off, jn));
        }
        ver = scrapeVersion(line);
        if (!ver.isEmpty() && !m_gotCfg) {
            m_gotCfg = true;
            emit connected(ver);
            emit status(QStringLiteral("Device connected."));
        }
        return;
    }
    if (std::strstr(line, "[Cfg]") != nullptr &&
        std::strstr(line, "crc") != nullptr) {
        if (m_pend == PendCfg) {
            m_pendTimer->stop();
            m_pend = PendNone;
        }
        emit status(QStringLiteral("Cfg CRC rejected."));
    }
}

void IoThread::onPendTimeout()
{
    if (m_pend == PendCfg) {
        emit status(QStringLiteral("No Cfg reply."));
    } else if (m_pend == PendTime && m_timeReport) {
        emit status(QStringLiteral("No Time reply."));
    }
    m_pend = PendNone;
    m_timeReport = false;
}

bool IoThread::callBusy() const
{
    return m_callOn;
}

void IoThread::disarmCall()
{
    if (m_callTimer != nullptr) {
        m_callTimer->stop();
    }
    if (!m_callOn) {
        return;
    }
    m_callOn = false;
    m_callListen = false;
    m_callFlight = 0;
    emit callArmed(false);
    emitCallState();
    emit callDown();
    emit status(QStringLiteral("Idle"));
}

static int callFlight(const struct gt_link *L)
{
    int n = 0;
    int i;

    for (i = 0; i < L->n_out; i++) {
        if (L->out[i].ns >= 0) {
            n++;
        }
    }
    return n;
}

static QString callPeer(const struct gt_link *L)
{
    char src[16];
    char dst[16];

    gt_ax25_fmt_call(src, sizeof(src), &L->o.src);
    if (L->o.accept_any && L->state == GT_LINK_IDLE) {
        return QStringLiteral("%1>ANY").arg(QString::fromLatin1(src));
    }
    gt_ax25_fmt_call(dst, sizeof(dst), &L->o.dst);
    return QStringLiteral("%1>%2").arg(QString::fromLatin1(src),
                                       QString::fromLatin1(dst));
}

void IoThread::emitCallState()
{
    const int n2 = m_call.o.n2 > 0 ? m_call.o.n2 : 10;

    emit callState(m_callOn ? gt_link_state(&m_call) : GT_LINK_IDLE,
                   m_callOn ? callPeer(&m_call) : QString(), m_callListen,
                   m_call.tries, n2);
}

void IoThread::noteCallFlight()
{
    const int fl = m_callOn ? callFlight(&m_call) : 0;

    if (fl < m_callFlight) {
        emit callAcked(m_callFlight - fl);
    }
    m_callFlight = fl;
}

void IoThread::emitCallEv(const struct gt_link_ev *ev)
{
    noteCallFlight();
    emitCallState();
    if (ev == nullptr) {
        return;
    }
    if (ev->type == GT_LINK_EV_UP) {
        char src[16];
        char dst[16];

        gt_ax25_fmt_call(src, sizeof(src), &m_call.o.src);
        gt_ax25_fmt_call(dst, sizeof(dst), &m_call.o.dst);
        emit callUp(QString::fromLatin1(src), QString::fromLatin1(dst));
        emit status(QStringLiteral("Call up %1>%2")
                        .arg(QString::fromLatin1(src), QString::fromLatin1(dst)));
    } else if (ev->type == GT_LINK_EV_INFO) {
        emit callInfo(QByteArray(reinterpret_cast<const char *>(ev->info),
                                 ev->info_len));
    } else if (ev->type == GT_LINK_EV_UI) {
        emit callUi(QByteArray(reinterpret_cast<const char *>(ev->info),
                               ev->info_len));
    } else if (ev->type == GT_LINK_EV_DOWN) {
        emit status(QStringLiteral("Call down."));
        emit callDown();
        if (m_callListen) {
            gt_link_init(&m_call, m_fd, &m_callOpts);
            m_callFlight = 0;
            emitCallState();
            emit status(QStringLiteral("Listening %1").arg(callPeer(&m_call)));
        } else {
            if (m_callTimer != nullptr) {
                m_callTimer->stop();
            }
            m_callOn = false;
            m_callListen = false;
            m_callFlight = 0;
            emit callArmed(false);
            emitCallState();
        }
    }
}

void IoThread::onCallTick()
{
    struct gt_link_ev ev;

    if (!m_callOn) {
        return;
    }
    if (gt_link_tick(&m_call, &ev) < 0) {
        emit callError(QStringLiteral("Call i/o error."));
        disarmCall();
        return;
    }
    emitCallEv(&ev);
}

void IoThread::startCall(const QString &src, const QString &dst,
                         const QString &path, bool listen, int t1, int t2,
                         int t3, int n2, int paclen, int maxframe, int txdelay,
                         int txtail)
{
    const QByteArray srcB = src.trimmed().toLatin1();
    const QByteArray dstB = dst.trimmed().toLatin1();
    const QByteArray pathB = path.trimmed().toLatin1();
    int n_digi = 0;
    const bool any = listen &&
                     (dstB.isEmpty() ||
                      QString::fromLatin1(dstB).compare(
                          QStringLiteral("ANY"), Qt::CaseInsensitive) == 0);

    if (m_fd < 0) {
        emit callError(QStringLiteral("Not connected."));
        return;
    }
    if (m_callOn) {
        emit callError(QStringLiteral("Call already active."));
        return;
    }
    if (m_lab != LabOff) {
        emit callError(QStringLiteral("Stop dump/inject first."));
        return;
    }
    std::memset(&m_callOpts, 0, sizeof(m_callOpts));
    if (gt_ax25_parse_call(srcB.constData(), &m_callOpts.src) != 0) {
        emit callError(QStringLiteral("Bad Call src."));
        return;
    }
    if (!any) {
        if (dstB.isEmpty() ||
            gt_ax25_parse_call(dstB.constData(), &m_callOpts.dst) != 0) {
            emit callError(QStringLiteral("Bad Call dst."));
            return;
        }
    }
    if (gt_ax25_parse_path(pathB.isEmpty() ? nullptr : pathB.constData(),
                           m_callOpts.digi, &n_digi) != 0) {
        emit callError(QStringLiteral("Bad Call path."));
        return;
    }
    m_callOpts.n_digi = n_digi;
    m_callOpts.accept_any = any ? 1 : 0;
    m_callOpts.t1_ms = t1;
    m_callOpts.t2_ms = t2;
    m_callOpts.t3_ms = t3;
    m_callOpts.n2 = n2;
    m_callOpts.paclen = paclen;
    m_callOpts.maxframe = maxframe;
    m_callOpts.txdelay = txdelay;
    m_callOpts.txtail = txtail;

    gt_link_init(&m_call, m_fd, &m_callOpts);
    m_callOn = true;
    m_callListen = listen;
    m_callFlight = 0;
    emit callArmed(true);
    if (!listen) {
        if (gt_link_start(&m_call) != 0) {
            emit callError(QStringLiteral("SABM write failed."));
            disarmCall();
            return;
        }
    }
    emitCallState();
    emit status(listen ? QStringLiteral("Listening %1").arg(callPeer(&m_call))
                       : QStringLiteral("Calling %1").arg(callPeer(&m_call)));
    m_callTimer->start(50);
}

void IoThread::stopCall()
{
    if (!m_callOn) {
        return;
    }
    {
        const int st = gt_link_state(&m_call);

        if (st == GT_LINK_CONNECTED || st == GT_LINK_SABM) {
            if (gt_link_disconnect(&m_call) != 0) {
                emit callError(QStringLiteral("DISC write failed."));
                disarmCall();
                return;
            }
            emitCallState();
            emit status(QStringLiteral("Idle"));
            return;
        }
    }
    disarmCall();
}

void IoThread::queueCallLine(const QByteArray &line)
{
    int q;

    if (!m_callOn || gt_link_state(&m_call) != GT_LINK_CONNECTED) {
        emit callError(QStringLiteral("Not connected."));
        return;
    }
    q = gt_link_queue(&m_call, 0xF0,
                      reinterpret_cast<const uint8_t *>(line.constData()),
                      line.size());
    if (q == 1) {
        noteCallFlight();
        emit callQueued(line);
        emitCallState();
        return;
    }
    if (q == 0) {
        emit callError(QStringLiteral("Window full."));
        return;
    }
    if (q == -2) {
        emit callError(QStringLiteral("Line longer than paclen."));
        return;
    }
    emit callError(QStringLiteral("Queue failed."));
    disarmCall();
}
