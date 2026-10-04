#ifndef IOTHREAD_H
#define IOTHREAD_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * IoThread.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QSocketNotifier;
class QTimer;

class IoThread : public QObject {
    Q_OBJECT

public:
    explicit IoThread(QObject *parent = nullptr);
    ~IoThread() override;

signals:
    void status(const QString &text);
    void connected(const QString &fwVersion);
    void disconnected();
    void logLine(const QString &line);
    void heardFrame(const QString &line);
    void aprsHeard(const QString &line);
    void cfgJson(const QString &json);
    void timeAck(quint64 hostMs, quint64 tncMs);
    void rms(unsigned value);
    void pcmChunk(const QByteArray &le16);
    void dumpActive(bool on);
    void injectActive(bool on);
    void injectStats(unsigned ax25, unsigned ok, unsigned fail);
    void analysisLine(const QString &line);
    void callArmed(bool on);
    void callState(int state, const QString &peer, bool listen, int tries,
                   int n2);
    void callUp(const QString &src, const QString &dst);
    void callDown();
    void callInfo(const QByteArray &info);
    void callUi(const QByteArray &info);
    void callQueued(const QByteArray &line);
    void callAcked(int n);
    void callError(const QString &text);

public slots:
    void startSession(const QString &path);
    void stopSession();
    void requestCfgGet();
    void requestCfgPut(const QString &json);
    void requestTimeSync();
    void requestTimeGet();
    void startDump();
    void stopDump();
    void startInject(const QString &wavPath);
    void stopInject();
    void sendKissData(const QByteArray &frame);
    void startCall(const QString &src, const QString &dst, const QString &path,
                   bool listen, int t1, int t2, int t3, int n2, int paclen,
                   int maxframe, int txdelay, int txtail);
    void stopCall();
    void queueCallLine(const QByteArray &line);

private slots:
    void onReady();
    void onPendTimeout();
    void onInjectTick();
    void onCallTick();

private:
    enum Pend {
        PendNone = 0,
        PendCfg,
        PendTime
    };
    enum Lab {
        LabOff = 0,
        LabDump,
        LabInject
    };

    void closeFd();
    void armPend(Pend p, int ms);
    void handleFrame();
    void handleLog(const char *line);
    bool writeHw(const uint8_t *pay, int n);
    bool writeCfgQuery();
    bool writeCfgPut(const QString &json);
    bool writeTime(bool sync, bool reportFail);
    bool writeLab(uint8_t hw);
    void endLab();
    void emitCallEv(const struct gt_link_ev *ev);
    void emitCallState();
    void noteCallFlight();
    void disarmCall();
    bool callBusy() const;

    int m_fd;
    kiss_rx_t m_rx;
    QSocketNotifier *m_not;
    QTimer *m_pendTimer;
    QTimer *m_injTimer;
    Pend m_pend;
    bool m_gotCfg;
    bool m_timeReport;
    Lab m_lab;
    int m_wavFd;
    uint32_t m_wavLeft;
    uint8_t m_injSeq;
    unsigned m_ax25;
    unsigned m_fcsOk;
    unsigned m_fcsFail;
    bool m_callOn;
    bool m_callListen;
    int m_callFlight;
    struct gt_link_opts m_callOpts;
    struct gt_link m_call;
    QTimer *m_callTimer;
};

#endif
