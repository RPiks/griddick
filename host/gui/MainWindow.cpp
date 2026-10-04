/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * MainWindow.cpp - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "MainWindow.h"
#include "IoThread.h"
#include "LevelBar.h"
#include "Waterfall.h"
#include "fft_r2.h"

#include "gt.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <QCheckBox>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QSplitter>
#include <QStorageInfo>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <unistd.h>

#include <QCoreApplication>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>

static const int kListCap = 500;

static const char *kLedOff =
    "background:#555; border-radius:8px; min-width:16px; max-width:16px;"
    "min-height:16px; max-height:16px;";
static const char *kLedOn =
    "background:#3c3; border-radius:8px; min-width:16px; max-width:16px;"
    "min-height:16px; max-height:16px;";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dev(nullptr)
    , m_actDark(nullptr)
    , m_actLight(nullptr)
    , m_actNight(nullptr)
    , m_connect(nullptr)
    , m_disconnect(nullptr)
    , m_led(nullptr)
    , m_fwVer(nullptr)
    , m_appVer(nullptr)
    , m_rms(nullptr)
    , m_status(nullptr)
    , m_log(nullptr)
    , m_heard(nullptr)
    , m_legacy(nullptr)
    , m_build(nullptr)
    , m_txdelay(nullptr)
    , m_persist(nullptr)
    , m_slottime(nullptr)
    , m_txtail(nullptr)
    , m_fulldup(nullptr)
    , m_linkmode(nullptr)
    , m_mycall(nullptr)
    , m_paclen(nullptr)
    , m_maxframe(nullptr)
    , m_t1(nullptr)
    , m_t2(nullptr)
    , m_t3(nullptr)
    , m_n2(nullptr)
    , m_clock(nullptr)
    , m_settingsBox(nullptr)
    , m_dumpOn(nullptr)
    , m_dumpOff(nullptr)
    , m_saveWav(nullptr)
    , m_loadWav(nullptr)
    , m_level(nullptr)
    , m_fall(nullptr)
    , m_bpp(nullptr)
    , m_speed(nullptr)
    , m_startHz(nullptr)
    , m_timeMarks(nullptr)
    , m_normalize(nullptr)
    , m_analysis(nullptr)
    , m_anList(nullptr)
    , m_uf2(nullptr)
    , m_rp2(nullptr)
    , m_flash(nullptr)
    , m_txSrc(nullptr)
    , m_txDst(nullptr)
    , m_txPath(nullptr)
    , m_txCtrl(nullptr)
    , m_txPid(nullptr)
    , m_txMode(nullptr)
    , m_txBody(nullptr)
    , m_txFile(nullptr)
    , m_txBrowse(nullptr)
    , m_txSend(nullptr)
    , m_txBeacon(nullptr)
    , m_txStop(nullptr)
    , m_txEvery(nullptr)
    , m_txCount(nullptr)
    , m_txTimer(nullptr)
    , m_txLeft(0)
    , m_txBox(nullptr)
    , m_aprsBox(nullptr)
    , m_aprsLat(nullptr)
    , m_aprsLon(nullptr)
    , m_aprsCmt(nullptr)
    , m_aprsStatus(nullptr)
    , m_aprsTo(nullptr)
    , m_aprsMsg(nullptr)
    , m_aprsDst(nullptr)
    , m_aprsPath(nullptr)
    , m_aprsSend(nullptr)
    , m_aprsBeacon(nullptr)
    , m_aprsStop(nullptr)
    , m_aprsEvery(nullptr)
    , m_aprsCount(nullptr)
    , m_aprsTimer(nullptr)
    , m_aprsLeft(0)
    , m_aprsLog(nullptr)
    , m_callBox(nullptr)
    , m_callSrc(nullptr)
    , m_callDst(nullptr)
    , m_callPath(nullptr)
    , m_callListen(nullptr)
    , m_callGo(nullptr)
    , m_callHang(nullptr)
    , m_callState(nullptr)
    , m_callLog(nullptr)
    , m_callLine(nullptr)
    , m_callSend(nullptr)
    , m_devUp(false)
    , m_callArmed(false)
    , m_callListening(false)
    , m_callSt(0)
    , m_callTries(0)
    , m_callN2(10)
    , m_thread(new QThread(this))
    , m_io(new IoThread())
    , m_theme(1)
    , m_statOp(0)
    , m_dumpOnDev(false)
    , m_lastBarMs(0)
    , m_lastRmsMs(0)
    , m_winSq(0.0)
    , m_winN(0)
    , m_winPeak(0.0f)
    , m_wav(nullptr)
    , m_wavN(0)
    , m_fftHop(8192)
    , m_normOn(false)
    , m_normReady(false)
    , m_normSamp(0)
    , m_normFrames(0)
    , m_clips(0)
{
    setWindowTitle(QStringLiteral("Griddick Station"));
    resize(960, 640);

    m_io->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_io, &QObject::deleteLater);
    m_thread->start();

    auto *root = new QWidget(this);
    auto *lay = new QVBoxLayout(root);

    auto *bar = new QHBoxLayout();
    bar->addWidget(new QLabel(QStringLiteral("Device")));
    m_dev = new QComboBox(this);
    m_dev->setFixedWidth(200);
    m_dev->setEditable(true);
    bar->addWidget(m_dev);
    auto *scan = new QPushButton(QStringLiteral("Scan"), this);
    m_connect = new QPushButton(QStringLiteral("Connect"), this);
    m_disconnect = new QPushButton(QStringLiteral("Disconnect"), this);
    bar->addWidget(scan);
    bar->addWidget(m_connect);
    bar->addWidget(m_disconnect);
    bar->addStretch(1);
    lay->addLayout(bar);

    auto *meta = new QHBoxLayout();
    m_led = new QLabel(this);
    m_led->setStyleSheet(kLedOff);
    meta->addWidget(m_led);
    meta->addWidget(new QLabel(QStringLiteral("Firmware")));
    m_fwVer = new QLabel(QStringLiteral("—"), this);
    meta->addWidget(m_fwVer);
    meta->addSpacing(16);
    meta->addWidget(new QLabel(QStringLiteral("App")));
    m_appVer = new QLabel(QString::fromLatin1(GT_VERSION), this);
    meta->addWidget(m_appVer);
    meta->addSpacing(16);
    meta->addWidget(new QLabel(QStringLiteral("IN")));
    m_rms = new QLabel(QStringLiteral("—"), this);
    meta->addWidget(m_rms);
    meta->addStretch(1);
    lay->addLayout(meta);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildMonitorTab(), QStringLiteral("LogMon"));
    tabs->addTab(buildWaterfallTab(), QStringLiteral("Waterfall"));
    /* tabs->addTab(buildTestTab(), QStringLiteral("Test")); */
    tabs->addTab(buildSettingsTab(), QStringLiteral("Config"));
    tabs->addTab(buildTxTab(), QStringLiteral("TX"));
    tabs->addTab(buildAprsTab(), QStringLiteral("APRS"));
    tabs->addTab(buildCallTab(), QStringLiteral("Call"));
    tabs->addTab(buildFlashTab(), QStringLiteral("Flash"));
    lay->addWidget(tabs, 1);

    m_status = new QLabel(QStringLiteral("Device disconnected."), this);
    lay->addWidget(m_status);

    setCentralWidget(root);

    auto *settings = menuBar()->addMenu(QStringLiteral("&Settings"));
    auto *theme = settings->addMenu(QStringLiteral("&Theme"));
    auto *grp = new QActionGroup(this);
    grp->setExclusive(true);
    m_actDark = theme->addAction(QStringLiteral("&Dark"));
    m_actNight = theme->addAction(QStringLiteral("&Night"));
    m_actLight = theme->addAction(QStringLiteral("&Light"));
    m_actDark->setCheckable(true);
    m_actNight->setCheckable(true);
    m_actLight->setCheckable(true);
    grp->addAction(m_actDark);
    grp->addAction(m_actNight);
    grp->addAction(m_actLight);
    m_actDark->setChecked(true);
    connect(m_actDark, &QAction::triggered, this, [this]() { setTheme(1); });
    connect(m_actNight, &QAction::triggered, this, [this]() { setTheme(2); });
    connect(m_actLight, &QAction::triggered, this, [this]() { setTheme(0); });

    auto *help = menuBar()->addMenu(QStringLiteral("&Help"));
    help->addAction(QStringLiteral("About Griddick Station"), this,
                    &MainWindow::onAbout);

    connect(scan, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(m_connect, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(m_disconnect, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(m_io, &IoThread::status, this, &MainWindow::onIoStatus);
    connect(m_io, &IoThread::connected, this, &MainWindow::onConnected);
    connect(m_io, &IoThread::disconnected, this, &MainWindow::onDisconnected);
    connect(m_io, &IoThread::logLine, this, &MainWindow::onLogLine);
    connect(m_io, &IoThread::heardFrame, this, &MainWindow::onHeardFrame);
    connect(m_io, &IoThread::aprsHeard, this, &MainWindow::onAprsHeard);
    connect(m_io, &IoThread::cfgJson, this, &MainWindow::onCfgJson);
    connect(m_io, &IoThread::timeAck, this, &MainWindow::onTimeAck);
    connect(m_io, &IoThread::rms, this, &MainWindow::onRms);
    connect(m_io, &IoThread::pcmChunk, this, &MainWindow::onPcmChunk);
    connect(m_io, &IoThread::dumpActive, this, &MainWindow::onDumpActive);
    connect(m_io, &IoThread::injectActive, this, &MainWindow::onInjectActive);
    connect(m_io, &IoThread::injectStats, this, &MainWindow::onInjectStats);
    connect(m_io, &IoThread::analysisLine, this, &MainWindow::onAnalysisLine);
    connect(m_io, &IoThread::callArmed, this, &MainWindow::onCallArmed);
    connect(m_io, &IoThread::callState, this, &MainWindow::onCallState);
    connect(m_io, &IoThread::callAcked, this, &MainWindow::onCallAcked);
    connect(m_io, &IoThread::callUp, this, &MainWindow::onCallUp);
    connect(m_io, &IoThread::callDown, this, &MainWindow::onCallDown);
    connect(m_io, &IoThread::callInfo, this, &MainWindow::onCallInfo);
    connect(m_io, &IoThread::callUi, this, &MainWindow::onCallUi);
    connect(m_io, &IoThread::callQueued, this, &MainWindow::onCallQueued);
    connect(m_io, &IoThread::callError, this, &MainWindow::onCallError);

    setConnectedUi(false);
    refreshDevices();
    loadUiSettings();
}

MainWindow::~MainWindow()
{
    saveUiSettings();
    closeWav();
    QMetaObject::invokeMethod(m_io, "stopSession", Qt::QueuedConnection);
    m_thread->quit();
    m_thread->wait(1500);
}

QWidget *MainWindow::buildTestTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);

    auto *btns = new QHBoxLayout();
    m_saveWav = new QPushButton(QStringLiteral("Save WAV…"), w);
    m_loadWav = new QPushButton(QStringLiteral("Load test WAV…"), w);
    btns->addWidget(m_saveWav);
    btns->addWidget(m_loadWav);
    btns->addStretch(1);
    lay->addLayout(btns);

    auto *an = new QGroupBox(QStringLiteral("Decode analysis"), w);
    auto *al = new QVBoxLayout(an);
    m_analysis = new QLabel(QStringLiteral("AX.25 0   FCS OK 0   fail 0"), an);
    m_anList = new QListWidget(an);
    al->addWidget(m_analysis);
    al->addWidget(m_anList);
    lay->addWidget(an, 1);

    connect(m_saveWav, &QPushButton::clicked, this, &MainWindow::onSaveWav);
    connect(m_loadWav, &QPushButton::clicked, this, &MainWindow::onLoadWav);
    return w;
}

QWidget *MainWindow::buildWaterfallTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);

    auto *btns = new QHBoxLayout();
    m_dumpOn = new QPushButton(QStringLiteral("Start"), w);
    m_dumpOff = new QPushButton(QStringLiteral("Stop"), w);
    m_dumpOff->setEnabled(false);
    m_bpp = new QComboBox(w);
    m_bpp->addItem(QStringLiteral("1/2"), 0.5);
    for (int i = 1; i <= 6; i++) {
        m_bpp->addItem(QString::number(i), static_cast<double>(i));
    }
    m_bpp->setCurrentIndex(0);
    m_speed = new QComboBox(w);
    m_speed->addItem(QStringLiteral("1x"), 1);
    m_speed->addItem(QStringLiteral("2x"), 2);
    m_speed->addItem(QStringLiteral("4x"), 4);
    m_speed->addItem(QStringLiteral("8x"), 8);
    m_speed->setCurrentIndex(0);
    m_startHz = new QSpinBox(w);
    m_startHz->setRange(0, 3500);
    m_startHz->setSingleStep(50);
    m_startHz->setSuffix(QStringLiteral(" Hz"));
    m_startHz->setValue(200);
    m_timeMarks = new QCheckBox(QStringLiteral("Time marks"), w);
    m_timeMarks->setChecked(true);
    m_normalize = new QCheckBox(QStringLiteral("Normalize"), w);
    m_normalize->setChecked(false);
    btns->addWidget(m_dumpOn);
    btns->addWidget(m_dumpOff);
    btns->addSpacing(12);
    btns->addWidget(new QLabel(QStringLiteral("Bins/Pixel"), w));
    btns->addWidget(m_bpp);
    btns->addWidget(new QLabel(QStringLiteral("Speed"), w));
    btns->addWidget(m_speed);
    btns->addWidget(new QLabel(QStringLiteral("Start"), w));
    btns->addWidget(m_startHz);
    btns->addWidget(m_timeMarks);
    btns->addWidget(m_normalize);
    btns->addStretch(1);
    lay->addLayout(btns);

    auto *row = new QHBoxLayout();
    m_level = new LevelBar(w);
    m_fall = new Waterfall(w);
    row->addWidget(m_level);
    row->addWidget(m_fall, 1);
    lay->addLayout(row, 1);

    connect(m_dumpOn, &QPushButton::clicked, this, &MainWindow::onDumpStart);
    connect(m_dumpOff, &QPushButton::clicked, this, &MainWindow::onDumpStop);
    connect(m_bpp, &QComboBox::currentIndexChanged, this,
            &MainWindow::onBinsPerPixel);
    connect(m_speed, &QComboBox::currentIndexChanged, this,
            &MainWindow::onFallSpeed);
    connect(m_startHz, &QSpinBox::valueChanged, this, &MainWindow::onStartHz);
    connect(m_timeMarks, &QCheckBox::toggled, this, &MainWindow::onTimeMarks);
    connect(m_normalize, &QCheckBox::toggled, this, &MainWindow::onNormalize);
    return w;
}

QWidget *MainWindow::buildTxTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    m_txBox = new QWidget(w);
    auto *box = new QVBoxLayout(m_txBox);

    auto *addr = new QHBoxLayout();
    addr->addWidget(new QLabel(QStringLiteral("Src"), m_txBox));
    m_txSrc = new QLineEdit(QStringLiteral("TEST-0"), m_txBox);
    m_txSrc->setMaximumWidth(120);
    addr->addWidget(m_txSrc);
    addr->addWidget(new QLabel(QStringLiteral("Dst"), m_txBox));
    m_txDst = new QLineEdit(QStringLiteral("TEST-0"), m_txBox);
    m_txDst->setMaximumWidth(120);
    addr->addWidget(m_txDst);
    addr->addWidget(new QLabel(QStringLiteral("Path"), m_txBox));
    m_txPath = new QLineEdit(m_txBox);
    m_txPath->setPlaceholderText(QStringLiteral("WIDE1-1,WIDE2-1"));
    addr->addWidget(m_txPath, 1);
    box->addLayout(addr);

    auto *ctl = new QHBoxLayout();
    ctl->addWidget(new QLabel(QStringLiteral("Ctrl"), m_txBox));
    m_txCtrl = new QSpinBox(m_txBox);
    m_txCtrl->setRange(0, 255);
    m_txCtrl->setDisplayIntegerBase(16);
    m_txCtrl->setPrefix(QStringLiteral("0x"));
    m_txCtrl->setValue(0x03);
    ctl->addWidget(m_txCtrl);
    ctl->addWidget(new QLabel(QStringLiteral("PID"), m_txBox));
    m_txPid = new QSpinBox(m_txBox);
    m_txPid->setRange(0, 255);
    m_txPid->setDisplayIntegerBase(16);
    m_txPid->setPrefix(QStringLiteral("0x"));
    m_txPid->setValue(0xF0);
    ctl->addWidget(m_txPid);
    ctl->addWidget(new QLabel(QStringLiteral("Payload"), m_txBox));
    m_txMode = new QComboBox(m_txBox);
    m_txMode->addItem(QStringLiteral("Text"), 0);
    m_txMode->addItem(QStringLiteral("Hex"), 1);
    m_txMode->addItem(QStringLiteral("File"), 2);
    m_txMode->addItem(QStringLiteral("Raw hex"), 3);
    m_txMode->addItem(QStringLiteral("Raw file"), 4);
    ctl->addWidget(m_txMode);
    ctl->addStretch(1);
    box->addLayout(ctl);

    m_txBody = new QPlainTextEdit(m_txBox);
    m_txBody->setPlaceholderText(QStringLiteral("GRIDDICK"));
    m_txBody->setMaximumHeight(120);
    box->addWidget(m_txBody);

    auto *file = new QHBoxLayout();
    m_txFile = new QLineEdit(m_txBox);
    m_txBrowse = new QPushButton(QStringLiteral("Browse…"), m_txBox);
    file->addWidget(m_txFile, 1);
    file->addWidget(m_txBrowse);
    box->addLayout(file);

    auto *go = new QHBoxLayout();
    m_txSend = new QPushButton(QStringLiteral("Send"), m_txBox);
    go->addWidget(m_txSend);
    go->addWidget(new QLabel(QStringLiteral("Every"), m_txBox));
    m_txEvery = new QSpinBox(m_txBox);
    m_txEvery->setRange(1, 3600);
    m_txEvery->setValue(30);
    m_txEvery->setSuffix(QStringLiteral(" s"));
    go->addWidget(m_txEvery);
    m_txBeacon = new QPushButton(QStringLiteral("Beacon"), m_txBox);
    m_txStop = new QPushButton(QStringLiteral("Stop"), m_txBox);
    m_txStop->setEnabled(false);
    go->addWidget(m_txBeacon);
    go->addWidget(m_txStop);
    go->addWidget(new QLabel(QStringLiteral("Count"), m_txBox));
    m_txCount = new QSpinBox(m_txBox);
    m_txCount->setRange(0, 9999);
    m_txCount->setSpecialValueText(QStringLiteral("∞"));
    m_txCount->setValue(0);
    go->addWidget(m_txCount);
    go->addStretch(1);
    box->addLayout(go);

    lay->addWidget(m_txBox);
    lay->addStretch(1);

    m_txTimer = new QTimer(this);
    connect(m_txTimer, &QTimer::timeout, this, &MainWindow::onTxSend);
    connect(m_txSend, &QPushButton::clicked, this, &MainWindow::onTxSend);
    connect(m_txBeacon, &QPushButton::clicked, this, &MainWindow::onTxBeacon);
    connect(m_txStop, &QPushButton::clicked, this, &MainWindow::onTxStop);
    connect(m_txMode, &QComboBox::currentIndexChanged, this,
            &MainWindow::onTxModeChanged);
    connect(m_txBrowse, &QPushButton::clicked, this, &MainWindow::onTxFileBrowse);
    onTxModeChanged();
    m_txBox->setEnabled(false);
    return w;
}

QWidget *MainWindow::buildCallTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    m_callBox = new QWidget(w);
    auto *box = new QVBoxLayout(m_callBox);

    auto *addr = new QHBoxLayout();
    addr->addWidget(new QLabel(QStringLiteral("Src"), m_callBox));
    m_callSrc = new QLineEdit(QStringLiteral("TEST-0"), m_callBox);
    m_callSrc->setMaximumWidth(120);
    addr->addWidget(m_callSrc);
    addr->addWidget(new QLabel(QStringLiteral("Dst"), m_callBox));
    m_callDst = new QLineEdit(m_callBox);
    m_callDst->setMaximumWidth(120);
    m_callDst->setPlaceholderText(QStringLiteral("CALL-0 or ANY"));
    addr->addWidget(m_callDst);
    addr->addWidget(new QLabel(QStringLiteral("Path"), m_callBox));
    m_callPath = new QLineEdit(m_callBox);
    m_callPath->setPlaceholderText(QStringLiteral("digis, comma-separated"));
    addr->addWidget(m_callPath, 1);
    box->addLayout(addr);

    auto *ctl = new QHBoxLayout();
    m_callGo = new QPushButton(QStringLiteral("Call"), m_callBox);
    m_callListen = new QPushButton(QStringLiteral("Listen"), m_callBox);
    m_callHang = new QPushButton(QStringLiteral("Hang Up"), m_callBox);
    m_callHang->setEnabled(false);
    m_callState = new QLabel(QStringLiteral("Idle"), m_callBox);
    m_callState->setFont(callMonoFont());
    ctl->addWidget(m_callGo);
    ctl->addWidget(m_callListen);
    ctl->addWidget(m_callHang);
    ctl->addSpacing(12);
    ctl->addWidget(m_callState, 1);
    box->addLayout(ctl);

    m_callLog = new QPlainTextEdit(m_callBox);
    m_callLog->setReadOnly(true);
    m_callLog->setMaximumBlockCount(kListCap);
    m_callLog->setFont(callMonoFont());
    box->addWidget(m_callLog, 1);

    auto *in = new QHBoxLayout();
    m_callLine = new QLineEdit(m_callBox);
    m_callLine->setFont(callMonoFont());
    m_callSend = new QPushButton(QStringLiteral("Send"), m_callBox);
    in->addWidget(m_callLine, 1);
    in->addWidget(m_callSend);
    box->addLayout(in);

    lay->addWidget(m_callBox);

    connect(m_callGo, &QPushButton::clicked, this, &MainWindow::onCallConnect);
    connect(m_callListen, &QPushButton::clicked, this, &MainWindow::onCallListen);
    connect(m_callHang, &QPushButton::clicked, this, &MainWindow::onCallHang);
    connect(m_callSend, &QPushButton::clicked, this, &MainWindow::onCallSend);
    connect(m_callLine, &QLineEdit::returnPressed, this, &MainWindow::onCallSend);
    return w;
}

QWidget *MainWindow::buildMonitorTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    m_log = new QListWidget(w);
    m_heard = new QListWidget(w);
    const QFont mono = callMonoFont();
    m_log->setFont(mono);
    m_heard->setFont(mono);
    auto *logs = new QGroupBox(QStringLiteral("Log"), w);
    auto *ll = new QVBoxLayout(logs);
    ll->addWidget(m_log);
    auto *fr = new QGroupBox(QStringLiteral("RX frames"), w);
    auto *fl = new QVBoxLayout(fr);
    fl->addWidget(m_heard);
    auto *split = new QSplitter(Qt::Vertical, w);
    split->addWidget(logs);
    split->addWidget(fr);
    split->setChildrenCollapsible(false);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);
    return w;
}

static QSpinBox *mkSpin(QWidget *parent, int lo, int hi, int val)
{
    auto *s = new QSpinBox(parent);
    s->setRange(lo, hi);
    s->setValue(val);
    return s;
}

QWidget *MainWindow::buildSettingsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    m_settingsBox = w;

    auto *build = new QGroupBox(QStringLiteral("Build"), w);
    auto *bl = new QVBoxLayout(build);
    m_build = new QLabel(QStringLiteral("version  —\nhash     —\nbuildUTC —"), build);
    m_build->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bl->addWidget(m_build);

    auto *kiss = new QGroupBox(QStringLiteral("KISS (RAM)"), w);
    auto *kf = new QFormLayout(kiss);
    m_legacy = new QCheckBox(QStringLiteral("legacyMode"), kiss);
    m_txdelay = mkSpin(kiss, 0, 255, 30);
    m_persist = mkSpin(kiss, 0, 255, 63);
    m_slottime = mkSpin(kiss, 0, 255, 0);
    m_txtail = mkSpin(kiss, 0, 255, 10);
    m_fulldup = new QCheckBox(kiss);
    kf->addRow(m_legacy);
    kf->addRow(QStringLiteral("txDelay (10 ms)"), m_txdelay);
    kf->addRow(QStringLiteral("persist"), m_persist);
    kf->addRow(QStringLiteral("slotTime (10 ms)"), m_slottime);
    kf->addRow(QStringLiteral("txTail (10 ms)"), m_txtail);
    kf->addRow(QStringLiteral("fullDup"), m_fulldup);

    auto *link = new QGroupBox(QStringLiteral("Link (RAM)"), w);
    auto *lf = new QFormLayout(link);
    m_linkmode = new QComboBox(link);
    m_linkmode->addItem(QStringLiteral("UI"), 0);
    m_linkmode->addItem(QStringLiteral("I"), 1);
    m_mycall = new QLineEdit(QStringLiteral("TEST-0"), link);
    m_mycall->setMaxLength(9);
    m_paclen = mkSpin(link, 1, 256, 256);
    m_maxframe = mkSpin(link, 1, 7, 4);
    m_t1 = mkSpin(link, 100, 60000, 2000);
    m_t2 = mkSpin(link, 0, 10000, 0);
    m_t3 = mkSpin(link, 0, 120000, 30000);
    m_n2 = mkSpin(link, 1, 16, 10);
    lf->addRow(QStringLiteral("linkMode"), m_linkmode);
    lf->addRow(QStringLiteral("myCall"), m_mycall);
    lf->addRow(QStringLiteral("paclen"), m_paclen);
    lf->addRow(QStringLiteral("maxframe"), m_maxframe);
    lf->addRow(QStringLiteral("t1_ms"), m_t1);
    lf->addRow(QStringLiteral("t2_ms"), m_t2);
    lf->addRow(QStringLiteral("t3_ms"), m_t3);
    lf->addRow(QStringLiteral("n2"), m_n2);

    auto *clk = new QGroupBox(QStringLiteral("Clock"), w);
    auto *cl = new QVBoxLayout(clk);
    m_clock = new QLabel(
        QStringLiteral("Host  —\nTNC   —\nΔ —"), clk);
    m_clock->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *cb = new QHBoxLayout();
    auto *sync = new QPushButton(QStringLiteral("Sync"), clk);
    auto *get = new QPushButton(QStringLiteral("Get"), clk);
    cb->addWidget(sync);
    cb->addWidget(get);
    cb->addStretch(1);
    cl->addWidget(m_clock);
    cl->addLayout(cb);

    auto *act = new QHBoxLayout();
    auto *reload = new QPushButton(QStringLiteral("Reload"), w);
    auto *apply = new QPushButton(QStringLiteral("Apply"), w);
    act->addWidget(reload);
    act->addWidget(apply);
    act->addStretch(1);

    auto *cols = new QHBoxLayout();
    cols->addWidget(kiss);
    cols->addWidget(link);
    lay->addWidget(build);
    lay->addLayout(cols);
    lay->addWidget(clk);
    lay->addLayout(act);
    lay->addStretch(1);

    connect(reload, &QPushButton::clicked, this, &MainWindow::onCfgGet);
    connect(apply, &QPushButton::clicked, this, &MainWindow::onCfgPut);
    connect(sync, &QPushButton::clicked, this, &MainWindow::onTimeSync);
    connect(get, &QPushButton::clicked, this, &MainWindow::onTimeGet);
    return w;
}

QWidget *MainWindow::buildFlashTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);

    auto *row = new QHBoxLayout();
    m_uf2 = new QLineEdit(w);
    m_uf2->setPlaceholderText(QStringLiteral("pre-built .uf2"));
    auto *browse = new QPushButton(QStringLiteral("Browse…"), w);
    row->addWidget(m_uf2, 1);
    row->addWidget(browse);
    lay->addLayout(row);

    auto *vol = new QHBoxLayout();
    vol->addWidget(new QLabel(QStringLiteral("RPI-RP2"), w));
    m_rp2 = new QLabel(QStringLiteral("not mounted"), w);
    auto *scan = new QPushButton(QStringLiteral("Scan"), w);
    m_flash = new QPushButton(QStringLiteral("Flash"), w);
    vol->addWidget(m_rp2, 1);
    vol->addWidget(scan);
    vol->addWidget(m_flash);
    lay->addLayout(vol);

    lay->addWidget(new QLabel(
        QStringLiteral("Hold BOOTSEL, plug USB, wait until RPI-RP2 is mounted, then Flash.\n"
                       "The app copies the UF2 onto that volume. Disconnect first if connected."),
        w));
    lay->addStretch(1);

    connect(browse, &QPushButton::clicked, this, &MainWindow::onFlashBrowse);
    connect(scan, &QPushButton::clicked, this, &MainWindow::onFlashScan);
    connect(m_flash, &QPushButton::clicked, this, &MainWindow::onFlash);
    connect(m_uf2, &QLineEdit::textChanged, this, [this](const QString &) {
        updateFlashReady();
    });
    return w;
}

QWidget *MainWindow::buildReservedTab(const QString &name)
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->addWidget(new QLabel(name + QStringLiteral(" — not in this build."), w));
    lay->addStretch(1);
    return w;
}

QWidget *MainWindow::buildAprsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    m_aprsBox = new QWidget(w);
    auto *box = new QVBoxLayout(m_aprsBox);

    auto *ll = new QHBoxLayout();
    ll->addWidget(new QLabel(QStringLiteral("Lat"), m_aprsBox));
    m_aprsLat = new QLineEdit(m_aprsBox);
    m_aprsLat->setPlaceholderText(QStringLiteral("55.7558"));
    m_aprsLat->setMaximumWidth(120);
    ll->addWidget(m_aprsLat);
    ll->addWidget(new QLabel(QStringLiteral("Lon"), m_aprsBox));
    m_aprsLon = new QLineEdit(m_aprsBox);
    m_aprsLon->setPlaceholderText(QStringLiteral("37.6173"));
    m_aprsLon->setMaximumWidth(120);
    ll->addWidget(m_aprsLon);
    ll->addWidget(new QLabel(QStringLiteral("Comment"), m_aprsBox));
    m_aprsCmt = new QLineEdit(m_aprsBox);
    ll->addWidget(m_aprsCmt, 1);
    box->addLayout(ll);

    auto *st = new QHBoxLayout();
    st->addWidget(new QLabel(QStringLiteral("Status"), m_aprsBox));
    m_aprsStatus = new QLineEdit(m_aprsBox);
    st->addWidget(m_aprsStatus, 1);
    box->addLayout(st);

    auto *msg = new QHBoxLayout();
    msg->addWidget(new QLabel(QStringLiteral("Msg to"), m_aprsBox));
    m_aprsTo = new QLineEdit(m_aprsBox);
    m_aprsTo->setPlaceholderText(QStringLiteral("ANY"));
    m_aprsTo->setMaximumWidth(120);
    msg->addWidget(m_aprsTo);
    msg->addWidget(new QLabel(QStringLiteral("Text"), m_aprsBox));
    m_aprsMsg = new QLineEdit(m_aprsBox);
    msg->addWidget(m_aprsMsg, 1);
    box->addLayout(msg);

    auto *addr = new QHBoxLayout();
    addr->addWidget(new QLabel(QStringLiteral("Dst"), m_aprsBox));
    m_aprsDst = new QLineEdit(QStringLiteral("APZ001"), m_aprsBox);
    m_aprsDst->setMaximumWidth(120);
    addr->addWidget(m_aprsDst);
    addr->addWidget(new QLabel(QStringLiteral("Path"), m_aprsBox));
    m_aprsPath = new QLineEdit(QStringLiteral("WIDE1-1,WIDE2-1"), m_aprsBox);
    addr->addWidget(m_aprsPath, 1);
    box->addLayout(addr);

    auto *go = new QHBoxLayout();
    m_aprsSend = new QPushButton(QStringLiteral("TX"), m_aprsBox);
    go->addWidget(m_aprsSend);
    go->addWidget(new QLabel(QStringLiteral("Every"), m_aprsBox));
    m_aprsEvery = new QSpinBox(m_aprsBox);
    m_aprsEvery->setRange(1, 3600);
    m_aprsEvery->setValue(30);
    m_aprsEvery->setSuffix(QStringLiteral(" s"));
    go->addWidget(m_aprsEvery);
    m_aprsBeacon = new QPushButton(QStringLiteral("Beacon"), m_aprsBox);
    m_aprsStop = new QPushButton(QStringLiteral("Stop"), m_aprsBox);
    m_aprsStop->setEnabled(false);
    go->addWidget(m_aprsBeacon);
    go->addWidget(m_aprsStop);
    go->addWidget(new QLabel(QStringLiteral("Count"), m_aprsBox));
    m_aprsCount = new QSpinBox(m_aprsBox);
    m_aprsCount->setRange(0, 9999);
    m_aprsCount->setSpecialValueText(QStringLiteral("∞"));
    m_aprsCount->setValue(0);
    go->addWidget(m_aprsCount);
    go->addStretch(1);
    box->addLayout(go);

    m_aprsLog = new QListWidget(m_aprsBox);
    m_aprsLog->setFont(callMonoFont());
    box->addWidget(m_aprsLog, 1);

    lay->addWidget(m_aprsBox);
    m_aprsTimer = new QTimer(this);
    connect(m_aprsTimer, &QTimer::timeout, this, &MainWindow::onAprsSend);
    connect(m_aprsSend, &QPushButton::clicked, this, &MainWindow::onAprsSend);
    connect(m_aprsBeacon, &QPushButton::clicked, this, &MainWindow::onAprsBeacon);
    connect(m_aprsStop, &QPushButton::clicked, this, &MainWindow::onAprsStop);
    m_aprsBox->setEnabled(false);
    return w;
}

void MainWindow::setLed(bool on)
{
    m_led->setStyleSheet(on ? kLedOn : kLedOff);
}

void MainWindow::updateWorkUi()
{
    const bool on = m_devUp;
    const bool call = m_callArmed;

    if (m_dumpOn != nullptr) {
        m_dumpOn->setEnabled(on && !call && !m_dumpOnDev);
        m_dumpOff->setEnabled(on && m_dumpOnDev);
    }
    if (m_saveWav != nullptr) {
        m_saveWav->setEnabled(on && !call);
    }
    if (m_loadWav != nullptr) {
        m_loadWav->setEnabled(on && !call);
    }
    if (m_settingsBox != nullptr) {
        m_settingsBox->setEnabled(on);
    }
    if (m_txBox != nullptr) {
        m_txBox->setEnabled(on && !call);
    }
    if (m_aprsBox != nullptr) {
        m_aprsBox->setEnabled(on && !call);
    }
    if (m_callBox != nullptr) {
        m_callGo->setEnabled(on && !call);
        m_callListen->setEnabled(on && !call);
        m_callHang->setEnabled(on && call);
        m_callLine->setEnabled(true);
        m_callSend->setEnabled(true);
    }
}

void MainWindow::setConnectedUi(bool on)
{
    m_devUp = on;
    m_connect->setEnabled(!on);
    m_disconnect->setEnabled(on);
    m_dev->setEnabled(!on);
    if (!on) {
        m_callArmed = false;
        m_callListening = false;
        m_callSt = GT_LINK_IDLE;
        setLed(false);
        m_fwVer->setText(QStringLiteral("—"));
        m_rms->setText(QStringLiteral("—"));
        m_clock->setText(QStringLiteral("Host  —\nTNC   —\nΔ —"));
        if (m_build != nullptr) {
            m_build->setText(QStringLiteral("version  —\nhash     —\nbuildUTC —"));
        }
        if (m_callState != nullptr) {
            m_callState->setText(QStringLiteral("Idle"));
        }
    }
    updateWorkUi();
}

void MainWindow::appendCapped(QListWidget *list, const QString &line)
{
    if (list == nullptr) {
        return;
    }
    list->addItem(line);
    while (list->count() > kListCap) {
        delete list->takeItem(0);
    }
    list->scrollToBottom();
}

static QByteArray slurpSys(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll().trimmed();
}

/* Raspberry Pi USB VID 2e8a. Display serial, else vid:pid.
 * Must canonicalize: /sys/class/tty/<node>/device is a symlink into
 * the USB tree. Walking the class path never hits idVendor, so every
 * ACM node was dropped as "unrelated".
 */
static QString picoHwId(const QString &node)
{
    QString cur = QFileInfo(QStringLiteral("/sys/class/tty/%1/device")
                                .arg(QFileInfo(node).fileName()))
                      .canonicalFilePath();
    int i;

    if (cur.isEmpty()) {
        return {};
    }
    for (i = 0; i < 10; i++) {
        const QByteArray vid = slurpSys(cur + QStringLiteral("/idVendor")).toLower();
        if (!vid.isEmpty()) {
            if (vid != "2e8a") {
                return {};
            }
            const QByteArray ser = slurpSys(cur + QStringLiteral("/serial"));
            if (!ser.isEmpty()) {
                return QString::fromLatin1(ser);
            }
            const QByteArray pid = slurpSys(cur + QStringLiteral("/idProduct")).toLower();
            return QStringLiteral("2e8a:%1").arg(QString::fromLatin1(pid));
        }
        QDir d(cur);
        if (!d.cdUp()) {
            break;
        }
        cur = d.absolutePath();
    }
    return {};
}

void MainWindow::refreshDevices()
{
    const QString keepPath = m_dev->currentData().toString().isEmpty()
                                 ? m_dev->currentText().trimmed()
                                 : m_dev->currentData().toString();
    m_dev->clear();

    const QDir dev(QStringLiteral("/dev"));
    const QStringList acms = dev.entryList(QStringList() << QStringLiteral("ttyACM*"),
                                           QDir::System, QDir::Name);
    for (const QString &n : acms) {
        const QString p = QStringLiteral("/dev/") + n;
        const QString id = picoHwId(p);
        if (id.isEmpty()) {
            continue;
        }
        if (m_dev->findData(p) >= 0) {
            continue;
        }
        m_dev->addItem(id, p);
    }

    int i = m_dev->findData(keepPath);
    if (i < 0) {
        i = m_dev->findText(keepPath);
    }
    if (i >= 0) {
        m_dev->setCurrentIndex(i);
    } else if (!keepPath.isEmpty() && m_dev->count() == 0) {
        m_dev->setEditText(keepPath);
    }
}

void MainWindow::onConnect()
{
    QString path = m_dev->currentData().toString();
    if (path.isEmpty()) {
        path = m_dev->currentText().trimmed();
    }
    if (path.isEmpty()) {
        m_status->setText(QStringLiteral("No device path."));
        return;
    }
    m_connect->setEnabled(false);
    m_disconnect->setEnabled(true);
    m_status->setText(QStringLiteral("Opening…"));
    QMetaObject::invokeMethod(m_io, "startSession", Qt::QueuedConnection,
                              Q_ARG(QString, path));
}

void MainWindow::onDisconnect()
{
    QMetaObject::invokeMethod(m_io, "stopSession", Qt::QueuedConnection);
}

void MainWindow::onIoStatus(const QString &text)
{
    m_status->setText(text);
}

void MainWindow::onConnected(const QString &fwVersion)
{
    setLed(true);
    m_fwVer->setText(fwVersion);
    setConnectedUi(true);
}

void MainWindow::onDisconnected()
{
    onTxStop();
    onAprsStop();
    setConnectedUi(false);
}

void MainWindow::onLogLine(const QString &line)
{
    appendCapped(m_log, line);
}

void MainWindow::onHeardFrame(const QString &line)
{
    appendCapped(m_heard, line);
}

void MainWindow::setActionStatus(const QString &text)
{
    if (m_status == nullptr) {
        return;
    }
    m_status->setText(text);
}

static QString jsonString(const QByteArray &js, const char *key)
{
    const char *p = std::strstr(js.constData(), key);
    const char *e;

    if (p == nullptr) {
        return {};
    }
    p = std::strchr(p + std::strlen(key), '"');
    if (p == nullptr) {
        return {};
    }
    p++;
    e = p;
    while (*e != '\0' && *e != '"') {
        if (*e == '\\' && e[1] != '\0') {
            e += 2;
        } else {
            e++;
        }
    }
    if (*e != '"') {
        return {};
    }
    return QString::fromLatin1(p, static_cast<int>(e - p));
}

void MainWindow::onCfgJson(const QString &json)
{
    struct gt_cfg c;
    QByteArray utf = json.toUtf8();

    if (gt_cfg_from_json(utf.constData(), &c) != 0) {
        return;
    }
    applyCfgToForm(&c);
    if (m_build != nullptr) {
        const QString ver = jsonString(utf, "\"version\"");
        const QString hash = jsonString(utf, "\"hash\"");
        const QString utc = jsonString(utf, "\"buildUTC\"");
        m_build->setText(QStringLiteral("version  %1\nhash     %2\nbuildUTC %3")
                             .arg(ver.isEmpty() ? QStringLiteral("—") : ver,
                                  hash.isEmpty() ? QStringLiteral("—") : hash,
                                  utc.isEmpty() ? QStringLiteral("—") : utc));
    }
    if (m_statOp == 1) {
        setActionStatus(QStringLiteral("Cfg loaded."));
    } else if (m_statOp == 2) {
        setActionStatus(QStringLiteral("Cfg applied."));
    }
    m_statOp = 0;
}

QString MainWindow::fmtClock(quint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ms))
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

void MainWindow::onTimeAck(quint64 hostMs, quint64 tncMs)
{
    const qint64 d = static_cast<qint64>(tncMs) - static_cast<qint64>(hostMs);
    const QString sign = (d > 0) ? QStringLiteral("+")
                                 : (d < 0) ? QStringLiteral("−")
                                           : QStringLiteral("");
    const qint64 ad = d < 0 ? -d : d;
    m_clock->setText(QStringLiteral("Host  %1\nTNC   %2\nΔ %3%4 ms")
                         .arg(fmtClock(hostMs), fmtClock(tncMs), sign,
                              QString::number(ad)));
    if (m_statOp == 3) {
        setActionStatus(QStringLiteral("Time synced."));
    } else if (m_statOp == 4) {
        setActionStatus(QStringLiteral("Time got."));
    }
    m_statOp = 0;
}

void MainWindow::onRms(unsigned value)
{
    if (m_dumpOnDev) {
        return;
    }
    if (m_rms == nullptr) {
        return;
    }
    if (value == 0) {
        m_rms->setText(QStringLiteral("n/a"));
        return;
    }
    const float db = 20.0f * std::log10(static_cast<float>(value) / 32768.0f);
    m_rms->setText(QStringLiteral("%1 dBFS").arg(db, 0, 'f', 1));
}

void MainWindow::applyCfgToForm(const struct gt_cfg *c)
{
    m_legacy->setChecked(c->legacy != 0);
    m_txdelay->setValue(c->txdelay);
    m_persist->setValue(c->persist);
    m_slottime->setValue(c->slottime);
    m_txtail->setValue(c->txtail);
    m_fulldup->setChecked(c->fulldup != 0);
    m_linkmode->setCurrentIndex(c->linkmode ? 1 : 0);
    m_mycall->setText(QString::fromLatin1(
        c->mycall[0] != '\0' ? c->mycall : "TEST-0"));
    m_paclen->setValue(c->paclen > 0 ? c->paclen : 256);
    m_maxframe->setValue(c->maxframe > 0 ? c->maxframe : 4);
    m_t1->setValue(c->t1_ms > 0 ? c->t1_ms : 2000);
    m_t2->setValue(c->t2_ms);
    m_t3->setValue(c->t3_ms > 0 ? c->t3_ms : 30000);
    m_n2->setValue(c->n2 > 0 ? c->n2 : 10);
    if (m_txSrc != nullptr && c->mycall[0] != '\0') {
        m_txSrc->setText(QString::fromLatin1(c->mycall));
    }
    if (m_callSrc != nullptr && c->mycall[0] != '\0') {
        m_callSrc->setText(QString::fromLatin1(c->mycall));
    }
}

bool MainWindow::formToCfg(struct gt_cfg *c) const
{
    QByteArray call = m_mycall->text().trimmed().toLatin1();

    std::memset(c, 0, sizeof(*c));
    c->legacy = m_legacy->isChecked() ? 1 : 0;
    c->txdelay = m_txdelay->value();
    c->persist = m_persist->value();
    c->slottime = m_slottime->value();
    c->txtail = m_txtail->value();
    c->fulldup = m_fulldup->isChecked() ? 1 : 0;
    c->linkmode = m_linkmode->currentData().toInt();
    if (call.size() >= static_cast<int>(sizeof(c->mycall))) {
        return false;
    }
    std::memcpy(c->mycall, call.constData(), static_cast<size_t>(call.size()));
    c->mycall[call.size()] = '\0';
    c->paclen = m_paclen->value();
    c->maxframe = m_maxframe->value();
    c->t1_ms = m_t1->value();
    c->t2_ms = m_t2->value();
    c->t3_ms = m_t3->value();
    c->n2 = m_n2->value();
    c->have_txdelay = c->have_persist = c->have_slottime = 1;
    c->have_txtail = c->have_fulldup = c->have_legacy = 1;
    c->have_linkmode = c->have_mycall = c->have_paclen = 1;
    c->have_maxframe = c->have_t1 = c->have_t2 = c->have_t3 = c->have_n2 = 1;
    return true;
}

void MainWindow::onCfgGet()
{
    m_statOp = 1;
    setActionStatus(QStringLiteral("Reload."));
    QMetaObject::invokeMethod(m_io, "requestCfgGet", Qt::QueuedConnection);
}

void MainWindow::onCfgPut()
{
    struct gt_cfg c;
    char js[1024];

    if (!formToCfg(&c)) {
        setActionStatus(QStringLiteral("myCall too long."));
        return;
    }
    if (gt_cfg_to_json(&c, js, static_cast<int>(sizeof(js))) < 0) {
        setActionStatus(QStringLiteral("Cannot build settings JSON."));
        return;
    }
    m_statOp = 2;
    setActionStatus(QStringLiteral("Apply."));
    QMetaObject::invokeMethod(m_io, "requestCfgPut", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromUtf8(js)));
}

void MainWindow::onTimeSync()
{
    m_statOp = 3;
    setActionStatus(QStringLiteral("Time sync."));
    QMetaObject::invokeMethod(m_io, "requestTimeSync", Qt::QueuedConnection);
}

void MainWindow::onTimeGet()
{
    m_statOp = 4;
    setActionStatus(QStringLiteral("Time get."));
    QMetaObject::invokeMethod(m_io, "requestTimeGet", Qt::QueuedConnection);
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this,
        QStringLiteral("About Griddick Station"),
        QStringLiteral(
            "Griddick Station (gtstation) %1\n"
            "Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], "
            "callsign R2BDY.\n"
            "https://www.qrz.com/db/r2bdy\n\n"
            "This program is free software: you can redistribute it and/or modify "
            "it under the terms of the GNU Lesser General Public License as published "
            "by the Free Software Foundation, either version 2.1 of the License, or "
            "(at your option) any later version.\n\n"
            "https://www.gnu.org/licenses/lgpl-2.1.html")
            .arg(QString::fromLatin1(GT_VERSION)));
}

static QString stationConfPath()
{
    return QDir::homePath() + QStringLiteral("/.griddick/station.conf");
}

void MainWindow::applyTheme()
{
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette pal;
    if (m_theme == 0) {
        pal = QStyleFactory::create(QStringLiteral("Fusion"))->standardPalette();
    } else {
        const QColor win = (m_theme == 2) ? QColor(0x10, 0x16, 0x1A)
                                          : QColor(45, 45, 48);
        const QColor pane = (m_theme == 2) ? QColor(0x1F, 0x2D, 0x3D)
                                           : QColor(32, 32, 35);
        const QColor alt = (m_theme == 2) ? QColor(0x1F, 0x2D, 0x3D)
                                          : QColor(50, 50, 54);
        const QColor btn = (m_theme == 2) ? QColor(0x1F, 0x2D, 0x3D)
                                          : QColor(55, 55, 60);
        pal.setColor(QPalette::Window, win);
        pal.setColor(QPalette::WindowText, QColor(230, 230, 230));
        pal.setColor(QPalette::Base, pane);
        pal.setColor(QPalette::AlternateBase, alt);
        pal.setColor(QPalette::Text, QColor(230, 230, 230));
        pal.setColor(QPalette::Button, btn);
        pal.setColor(QPalette::ButtonText, QColor(230, 230, 230));
        pal.setColor(QPalette::Highlight, QColor(50, 110, 180));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::ToolTipBase, win);
        pal.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
        pal.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
    }
    qApp->setPalette(pal);
    for (const CallOut &o : m_callOut) {
        if (o.st == 2) {
            recolorCallBlock(o.block, callDoneColor());
        }
    }
}

void MainWindow::loadUiSettings()
{
    QSettings s(stationConfPath(), QSettings::IniFormat);
    {
        const QString th =
            s.value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString();
        if (th == QStringLiteral("light")) {
            m_theme = 0;
        } else if (th == QStringLiteral("night")) {
            m_theme = 2;
        } else {
            m_theme = 1;
        }
    }
    applyTheme();
    if (m_actDark != nullptr) {
        m_actLight->setChecked(m_theme == 0);
        m_actDark->setChecked(m_theme == 1);
        m_actNight->setChecked(m_theme == 2);
    }
    const QString dev = s.value(QStringLiteral("ui/device")).toString();
    if (!dev.isEmpty()) {
        int i = m_dev->findData(dev);
        if (i < 0) {
            i = m_dev->findText(dev);
        }
        if (i >= 0) {
            m_dev->setCurrentIndex(i);
        } else {
            m_dev->setEditText(dev);
        }
    }
    if (m_uf2 != nullptr) {
        const QString uf2 = s.value(QStringLiteral("ui/uf2")).toString();
        if (!uf2.isEmpty() && QFileInfo::exists(uf2)) {
            m_uf2->setText(uf2);
        } else {
            const QString found = findDefaultUf2();
            if (!found.isEmpty()) {
                m_uf2->setText(found);
            }
        }
        onFlashScan();
    }
    if (m_callSrc != nullptr) {
        const QString src = s.value(QStringLiteral("ui/callSrc")).toString();
        if (!src.isEmpty()) {
            m_callSrc->setText(src);
        }
        m_callDst->setText(s.value(QStringLiteral("ui/callDst")).toString());
        m_callPath->setText(s.value(QStringLiteral("ui/callPath")).toString());
    }
    if (m_speed != nullptr) {
        int speed = s.value(QStringLiteral("ui/fallSpeed"), 1).toInt();
        if (speed != 1 && speed != 2 && speed != 4 && speed != 8) {
            speed = 1;
        }
        const int i = m_speed->findData(speed);
        if (i >= 0) {
            m_speed->setCurrentIndex(i);
        }
    }
    if (m_bpp != nullptr) {
        const int i = m_bpp->findData(
            s.value(QStringLiteral("ui/fallBpp"), 0.5).toDouble());
        if (i >= 0) {
            m_bpp->setCurrentIndex(i);
        }
    }
    if (m_startHz != nullptr) {
        m_startHz->setValue(
            s.value(QStringLiteral("ui/fallStartHz"), 200).toInt());
    }
    if (m_timeMarks != nullptr) {
        m_timeMarks->setChecked(
            s.value(QStringLiteral("ui/fallTimeMarks"), true).toBool());
    }
    if (m_normalize != nullptr) {
        m_normalize->setChecked(
            s.value(QStringLiteral("ui/fallNorm"), false).toBool());
    }
    if (m_aprsLat != nullptr) {
        m_aprsLat->setText(s.value(QStringLiteral("ui/aprsLat")).toString());
        m_aprsLon->setText(s.value(QStringLiteral("ui/aprsLon")).toString());
        m_aprsCmt->setText(s.value(QStringLiteral("ui/aprsCmt")).toString());
        m_aprsStatus->setText(s.value(QStringLiteral("ui/aprsStatus")).toString());
        m_aprsTo->setText(s.value(QStringLiteral("ui/aprsTo")).toString());
        m_aprsMsg->setText(s.value(QStringLiteral("ui/aprsMsg")).toString());
        {
            const QString dst = s.value(QStringLiteral("ui/aprsDst")).toString();
            if (!dst.isEmpty()) {
                m_aprsDst->setText(dst);
            }
        }
        {
            const QString path = s.value(QStringLiteral("ui/aprsPath")).toString();
            if (!path.isEmpty()) {
                m_aprsPath->setText(path);
            }
        }
        m_aprsEvery->setValue(s.value(QStringLiteral("ui/aprsEvery"), 30).toInt());
        m_aprsCount->setValue(s.value(QStringLiteral("ui/aprsCount"), 0).toInt());
    }
}

void MainWindow::saveUiSettings()
{
    QDir().mkpath(QDir::homePath() + QStringLiteral("/.griddick"));
    QSettings s(stationConfPath(), QSettings::IniFormat);
    s.setValue(QStringLiteral("ui/theme"),
               m_theme == 0 ? QStringLiteral("light")
                            : m_theme == 2 ? QStringLiteral("night")
                                           : QStringLiteral("dark"));
    {
        QString path = m_dev->currentData().toString();
        if (path.isEmpty()) {
            path = m_dev->currentText().trimmed();
        }
        s.setValue(QStringLiteral("ui/device"), path);
    }
    if (m_uf2 != nullptr) {
        s.setValue(QStringLiteral("ui/uf2"), m_uf2->text().trimmed());
    }
    if (m_callSrc != nullptr) {
        s.setValue(QStringLiteral("ui/callSrc"), m_callSrc->text().trimmed());
        s.setValue(QStringLiteral("ui/callDst"), m_callDst->text().trimmed());
        s.setValue(QStringLiteral("ui/callPath"), m_callPath->text().trimmed());
    }
    if (m_speed != nullptr) {
        s.setValue(QStringLiteral("ui/fallSpeed"),
                   m_speed->currentData().toInt());
    }
    if (m_bpp != nullptr) {
        s.setValue(QStringLiteral("ui/fallBpp"), m_bpp->currentData().toDouble());
    }
    if (m_startHz != nullptr) {
        s.setValue(QStringLiteral("ui/fallStartHz"), m_startHz->value());
    }
    if (m_timeMarks != nullptr) {
        s.setValue(QStringLiteral("ui/fallTimeMarks"), m_timeMarks->isChecked());
    }
    if (m_normalize != nullptr) {
        s.setValue(QStringLiteral("ui/fallNorm"), m_normalize->isChecked());
    }
    if (m_aprsLat != nullptr) {
        s.setValue(QStringLiteral("ui/aprsLat"), m_aprsLat->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsLon"), m_aprsLon->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsCmt"), m_aprsCmt->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsStatus"),
                   m_aprsStatus->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsTo"), m_aprsTo->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsMsg"), m_aprsMsg->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsDst"), m_aprsDst->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsPath"), m_aprsPath->text().trimmed());
        s.setValue(QStringLiteral("ui/aprsEvery"), m_aprsEvery->value());
        s.setValue(QStringLiteral("ui/aprsCount"), m_aprsCount->value());
    }
}

void MainWindow::setTheme(int id)
{
    m_theme = id;
    applyTheme();
    saveUiSettings();
}

QString MainWindow::findRp2Volume() const
{
    const auto vols = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &s : vols) {
        if (!s.isReady() || s.isReadOnly()) {
            continue;
        }
        if (s.name() == QStringLiteral("RPI-RP2") ||
            s.displayName().contains(QStringLiteral("RPI-RP2")) ||
            s.rootPath().contains(QStringLiteral("RPI-RP2"))) {
            return s.rootPath();
        }
    }
    return {};
}

QString MainWindow::findDefaultUf2() const
{
    const QString name = QStringLiteral("griddick_tnc.uf2");
    QStringList dirs;
    const QString app = QCoreApplication::applicationDirPath();
    dirs << app
         << app + QStringLiteral("/firmware")
         << app + QStringLiteral("/../firmware")
         << QStringLiteral("/opt/griddick/station")
         << QStringLiteral("/opt/griddick/station/firmware")
         << QDir::currentPath()
         << app + QStringLiteral("/../..")
         << app + QStringLiteral("/../../build/firmware");
    for (const QString &d : dirs) {
        const QString p = QDir(d).absoluteFilePath(name);
        if (QFileInfo::exists(p)) {
            return QFileInfo(p).absoluteFilePath();
        }
    }
    return {};
}

void MainWindow::updateFlashReady()
{
    if (m_flash == nullptr) {
        return;
    }
    const QString uf2 = m_uf2 != nullptr ? m_uf2->text().trimmed() : QString();
    const bool haveUf2 = uf2.endsWith(QStringLiteral(".uf2"), Qt::CaseInsensitive) &&
                         QFileInfo::exists(uf2);
    const bool haveVol = m_rp2 != nullptr &&
                         m_rp2->property("path").toString().startsWith(QLatin1Char('/'));
    m_flash->setEnabled(haveUf2 && haveVol);
}

void MainWindow::onFlashBrowse()
{
    const QString f = QFileDialog::getOpenFileName(
        this, QStringLiteral("UF2"),
        m_uf2 != nullptr ? m_uf2->text() : QString(),
        QStringLiteral("UF2 (*.uf2);;All (*)"));
    if (!f.isEmpty() && m_uf2 != nullptr) {
        m_uf2->setText(f);
    }
}

void MainWindow::onFlashScan()
{
    const QString vol = findRp2Volume();
    if (m_rp2 == nullptr) {
        return;
    }
    if (vol.isEmpty()) {
        m_rp2->setText(QStringLiteral("not mounted"));
        m_rp2->setProperty("path", QString());
    } else {
        m_rp2->setText(vol);
        m_rp2->setProperty("path", vol);
    }
    updateFlashReady();
}

void MainWindow::onFlash()
{
    const QString srcPath = m_uf2 != nullptr ? m_uf2->text().trimmed() : QString();
    const QString vol = m_rp2 != nullptr ? m_rp2->property("path").toString() : QString();
    if (srcPath.isEmpty() || !QFileInfo::exists(srcPath)) {
        m_status->setText(QStringLiteral("No UF2 file."));
        return;
    }
    if (vol.isEmpty()) {
        onFlashScan();
        if (m_rp2->property("path").toString().isEmpty()) {
            m_status->setText(QStringLiteral("RPI-RP2 is not mounted."));
            return;
        }
    }
    const QString destVol = m_rp2->property("path").toString();
    const QString destPath = destVol + QLatin1Char('/') + QFileInfo(srcPath).fileName();

    if (m_dumpOnDev) {
        QMetaObject::invokeMethod(m_io, "stopDump", Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(m_io, "stopSession", Qt::QueuedConnection);

    QFile src(srcPath);
    if (!src.open(QIODevice::ReadOnly)) {
        m_status->setText(QStringLiteral("Cannot read UF2."));
        return;
    }
    QFile dst(destPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_status->setText(QStringLiteral("Cannot write %1").arg(destVol));
        return;
    }
    char buf[64 * 1024];
    qint64 n;
    while ((n = src.read(buf, sizeof(buf))) > 0) {
        if (dst.write(buf, n) != n) {
            m_status->setText(QStringLiteral("UF2 copy failed."));
            dst.close();
            src.close();
            return;
        }
    }
    dst.flush();
    if (dst.handle() >= 0) {
        (void)::fsync(dst.handle());
    }
    dst.close();
    src.close();
    ::sync();

    m_status->setText(QStringLiteral("UF2 written to %1. Pico will reboot.").arg(destVol));
    saveUiSettings();
    QTimer::singleShot(1500, this, &MainWindow::onFlashScan);
}

void MainWindow::onTxModeChanged()
{
    const int mode = m_txMode != nullptr ? m_txMode->currentData().toInt() : 0;
    const bool raw = (mode >= 3);
    const bool file = (mode == 2 || mode == 4);
    if (m_txSrc != nullptr) {
        m_txSrc->setEnabled(!raw);
        m_txDst->setEnabled(!raw);
        m_txPath->setEnabled(!raw);
        m_txCtrl->setEnabled(!raw);
        m_txPid->setEnabled(!raw);
    }
    if (m_txBody != nullptr) {
        m_txBody->setEnabled(!file);
    }
    if (m_txFile != nullptr) {
        m_txFile->setEnabled(file);
        m_txBrowse->setEnabled(file);
    }
}

void MainWindow::onTxFileBrowse()
{
    const QString f = QFileDialog::getOpenFileName(
        this, QStringLiteral("Payload file"), QString(),
        QStringLiteral("All (*)"));
    if (!f.isEmpty() && m_txFile != nullptr) {
        m_txFile->setText(f);
    }
}

bool MainWindow::buildTxFrame(QByteArray *out, QString *err) const
{
    uint8_t buf[512];
    int n = 0;
    const int mode = m_txMode->currentData().toInt();

    if (mode >= 3) {
        if (mode == 3) {
            const QByteArray hex = m_txBody->toPlainText().toLatin1();
            n = gt_parse_hex_blob(hex.constData(), buf, static_cast<int>(sizeof(buf)));
            if (n < 16) {
                *err = QStringLiteral("Raw hex invalid or shorter than 16 bytes.");
                return false;
            }
        } else {
            const QByteArray path = m_txFile->text().trimmed().toLocal8Bit();
            n = gt_load_file(path.constData(), buf, static_cast<int>(sizeof(buf)));
            if (n == -2) {
                *err = QStringLiteral("Raw file too large.");
                return false;
            }
            if (n < 16) {
                *err = QStringLiteral("Raw file missing or too short.");
                return false;
            }
        }
        out->resize(n);
        std::memcpy(out->data(), buf, static_cast<size_t>(n));
        return true;
    }

    struct gt_ax25_addr src;
    struct gt_ax25_addr dst;
    struct gt_ax25_addr digi[8];
    int n_digi = 0;
    uint8_t info[GT_AX25_INFO_MAX];
    int info_len = 0;
    const QByteArray src_s = m_txSrc->text().trimmed().toLatin1();
    const QByteArray dst_s = m_txDst->text().trimmed().toLatin1();
    const QByteArray path_s = m_txPath->text().trimmed().toLatin1();

    if (gt_ax25_parse_call(src_s.constData(), &src) != 0) {
        *err = QStringLiteral("Bad source call.");
        return false;
    }
    if (gt_ax25_parse_call(dst_s.constData(), &dst) != 0) {
        *err = QStringLiteral("Bad destination call.");
        return false;
    }
    if (!path_s.isEmpty() &&
        gt_ax25_parse_path(path_s.constData(), digi, &n_digi) != 0) {
        *err = QStringLiteral("Bad digi path.");
        return false;
    }
    if (mode == 1) {
        const QByteArray hex = m_txBody->toPlainText().toLatin1();
        info_len = gt_parse_hex_blob(hex.constData(), info, GT_AX25_INFO_MAX);
        if (info_len < 0) {
            *err = QStringLiteral("Invalid info hex.");
            return false;
        }
    } else if (mode == 2) {
        const QByteArray path = m_txFile->text().trimmed().toLocal8Bit();
        info_len = gt_load_file(path.constData(), info, GT_AX25_INFO_MAX);
        if (info_len == -2) {
            *err = QStringLiteral("Info file longer than 256.");
            return false;
        }
        if (info_len < 0) {
            *err = QStringLiteral("Cannot read info file.");
            return false;
        }
    } else {
        QByteArray text = m_txBody->toPlainText().toUtf8();
        if (text.isEmpty()) {
            text = QByteArrayLiteral("GRIDDICK");
        }
        if (text.size() > GT_AX25_INFO_MAX) {
            *err = QStringLiteral("Info longer than 256.");
            return false;
        }
        info_len = text.size();
        std::memcpy(info, text.constData(), static_cast<size_t>(info_len));
    }
    n = gt_ax25_build(buf, static_cast<int>(sizeof(buf)), &dst, &src, digi, n_digi,
                      static_cast<uint8_t>(m_txCtrl->value()),
                      static_cast<uint8_t>(m_txPid->value()),
                      info, info_len);
    if (n < 0) {
        *err = QStringLiteral("Frame too large.");
        return false;
    }
    out->resize(n);
    std::memcpy(out->data(), buf, static_cast<size_t>(n));
    return true;
}

void MainWindow::onTxSend()
{
    QByteArray frame;
    QString err;

    if (!buildTxFrame(&frame, &err)) {
        m_status->setText(err);
        if (m_txTimer != nullptr && m_txTimer->isActive()) {
            onTxStop();
        }
        return;
    }
    QMetaObject::invokeMethod(m_io, "sendKissData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, frame));
    if (m_txTimer != nullptr && m_txTimer->isActive() && m_txLeft > 0) {
        m_txLeft--;
        if (m_txLeft == 0) {
            onTxStop();
        }
    }
}

void MainWindow::onTxBeacon()
{
    if (m_txTimer == nullptr) {
        return;
    }
    m_txLeft = m_txCount->value();
    m_txTimer->start(m_txEvery->value() * 1000);
    m_txBeacon->setEnabled(false);
    m_txStop->setEnabled(true);
    onTxSend();
}

void MainWindow::onTxStop()
{
    if (m_txTimer != nullptr) {
        m_txTimer->stop();
    }
    m_txLeft = 0;
    if (m_txBeacon != nullptr) {
        m_txBeacon->setEnabled(true);
    }
    if (m_txStop != nullptr) {
        m_txStop->setEnabled(false);
    }
}

bool MainWindow::buildAprsFrame(QByteArray *out, QString *err) const
{
    uint8_t buf[512];
    uint8_t info[GT_AX25_INFO_MAX];
    int info_len = -1;
    struct gt_ax25_addr src;
    struct gt_ax25_addr dst;
    struct gt_ax25_addr digi[8];
    int n_digi = 0;
    int n;
    const QByteArray src_s = (m_mycall != nullptr)
                                 ? m_mycall->text().trimmed().toLatin1()
                                 : QByteArrayLiteral("TEST-0");
    const QByteArray dst_s = m_aprsDst->text().trimmed().toLatin1();
    const QByteArray path_s = m_aprsPath->text().trimmed().toLatin1();
    const QByteArray lat = m_aprsLat->text().trimmed().toLatin1();
    const QByteArray lon = m_aprsLon->text().trimmed().toLatin1();
    const QByteArray cmt = m_aprsCmt->text().trimmed().toUtf8();
    const QByteArray status = m_aprsStatus->text().trimmed().toUtf8();
    const QByteArray body = m_aprsMsg->text().trimmed().toUtf8();
    QByteArray to = m_aprsTo->text().trimmed().toLatin1();

    if (src_s.isEmpty() || gt_ax25_parse_call(src_s.constData(), &src) != 0) {
        *err = QStringLiteral("Bad myCall.");
        return false;
    }
    if (dst_s.isEmpty() || gt_ax25_parse_call(dst_s.constData(), &dst) != 0) {
        *err = QStringLiteral("Bad APRS dest.");
        return false;
    }
    if (!path_s.isEmpty() &&
        gt_ax25_parse_path(path_s.constData(), digi, &n_digi) != 0) {
        *err = QStringLiteral("Bad APRS path.");
        return false;
    }
    if (!lat.isEmpty() && !lon.isEmpty()) {
        const QByteArray ll = lat + ',' + lon;
        info_len = gt_aprs_build_pos(info, GT_AX25_INFO_MAX, ll.constData(),
                                     "/-", cmt.isEmpty() ? nullptr
                                                         : cmt.constData());
        if (info_len < 0) {
            *err = QStringLiteral("Bad lat/lon.");
            return false;
        }
    } else if (!status.isEmpty()) {
        info_len = gt_aprs_build_status(info, GT_AX25_INFO_MAX,
                                        status.constData());
        if (info_len < 0) {
            *err = QStringLiteral("Status too long.");
            return false;
        }
    } else if (!body.isEmpty()) {
        if (to.isEmpty() || to == QByteArrayLiteral("ANY")) {
            to = QByteArrayLiteral("APZ001");
        }
        info_len = gt_aprs_build_msg(info, GT_AX25_INFO_MAX, to.constData(),
                                     body.constData());
        if (info_len < 0) {
            *err = QStringLiteral("Bad APRS message.");
            return false;
        }
    } else {
        *err = QStringLiteral("Fill lat/lon, status, or message.");
        return false;
    }
    n = gt_ax25_build(buf, static_cast<int>(sizeof(buf)), &dst, &src, digi,
                      n_digi, 0x03, 0xF0, info, info_len);
    if (n < 0) {
        *err = QStringLiteral("APRS frame too large.");
        return false;
    }
    out->resize(n);
    std::memcpy(out->data(), buf, static_cast<size_t>(n));
    return true;
}

void MainWindow::onAprsSend()
{
    QByteArray frame;
    QString err;

    if (!buildAprsFrame(&frame, &err)) {
        setActionStatus(err);
        if (m_aprsTimer != nullptr && m_aprsTimer->isActive()) {
            onAprsStop();
        }
        return;
    }
    setActionStatus(QStringLiteral("APRS TX."));
    QMetaObject::invokeMethod(m_io, "sendKissData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, frame));
    if (m_aprsTimer != nullptr && m_aprsTimer->isActive() && m_aprsLeft > 0) {
        m_aprsLeft--;
        if (m_aprsLeft == 0) {
            onAprsStop();
        }
    }
}

void MainWindow::onAprsBeacon()
{
    if (m_aprsTimer == nullptr) {
        return;
    }
    m_aprsLeft = m_aprsCount->value();
    m_aprsTimer->start(m_aprsEvery->value() * 1000);
    m_aprsBeacon->setEnabled(false);
    m_aprsStop->setEnabled(true);
    onAprsSend();
}

void MainWindow::onAprsStop()
{
    if (m_aprsTimer != nullptr) {
        m_aprsTimer->stop();
    }
    m_aprsLeft = 0;
    if (m_aprsBeacon != nullptr) {
        m_aprsBeacon->setEnabled(true);
    }
    if (m_aprsStop != nullptr) {
        m_aprsStop->setEnabled(false);
    }
}

void MainWindow::onAprsHeard(const QString &line)
{
    appendCapped(m_aprsLog, line);
}

void MainWindow::onBinsPerPixel(int index)
{
    if (m_fall == nullptr || m_bpp == nullptr) {
        return;
    }
    m_fall->setBinsPerPixel(m_bpp->itemData(index).toDouble());
}

void MainWindow::onFallSpeed(int index)
{
    int speed = 1;
    if (m_speed != nullptr) {
        speed = m_speed->itemData(index).toInt();
    }
    if (speed != 1 && speed != 2 && speed != 4 && speed != 8) {
        speed = 1;
    }
    m_fftHop = 8192 / speed;
    m_acc.clear();
    if (m_fall != nullptr) {
        m_fall->clear();
    }
}

void MainWindow::onStartHz(int hz)
{
    if (m_fall != nullptr) {
        m_fall->setStartHz(hz);
    }
}

void MainWindow::onTimeMarks(bool on)
{
    if (m_fall != nullptr) {
        m_fall->setTimeMarks(on);
    }
}

void MainWindow::resetNorm()
{
    m_normReady = false;
    m_normSamp = 0;
    m_normFrames = 0;
    m_normSum.clear();
    m_normMean.clear();
}

void MainWindow::setNormStatus(bool accumulating)
{
    if (m_status == nullptr) {
        return;
    }
    static const QString kAcc = QStringLiteral("Accumulating data...");
    if (accumulating) {
        const QString cur = m_status->text();
        if (cur != kAcc) {
            m_statusHold = cur;
        }
        m_status->setText(kAcc);
    } else if (m_status->text() == kAcc) {
        m_status->setText(m_statusHold);
    }
}

void MainWindow::onNormalize(bool on)
{
    m_normOn = on;
    resetNorm();
    if (!on) {
        setNormStatus(false);
    }
}

void MainWindow::onDumpStart()
{
    m_clips = 0;
    m_winSq = 0.0;
    m_winN = 0;
    m_winPeak = 0.0f;
    m_acc.clear();
    if (m_fall != nullptr) {
        m_fall->clear();
    }
    QMetaObject::invokeMethod(m_io, "startDump", Qt::QueuedConnection);
}

void MainWindow::onDumpStop()
{
    closeWav();
    QMetaObject::invokeMethod(m_io, "stopDump", Qt::QueuedConnection);
}

void MainWindow::closeWav()
{
    if (m_wav == nullptr) {
        return;
    }
    if (m_wav->seek(0)) {
        uint8_t hdr[44];
        gt_wav_hdr_48k_mono(hdr, m_wavN);
        m_wav->write(reinterpret_cast<const char *>(hdr), 44);
    }
    m_wav->close();
    delete m_wav;
    m_wav = nullptr;
}

void MainWindow::onSaveWav()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save WAV"), QString(),
        QStringLiteral("WAV (*.wav);;All (*)"));
    if (path.isEmpty()) {
        return;
    }
    closeWav();
    m_wav = new QFile(path, this);
    if (!m_wav->open(QIODevice::WriteOnly)) {
        m_status->setText(QStringLiteral("Cannot write WAV."));
        delete m_wav;
        m_wav = nullptr;
        return;
    }
    uint8_t hdr[44];
    gt_wav_hdr_48k_mono(hdr, 0);
    m_wav->write(reinterpret_cast<const char *>(hdr), 44);
    m_wavN = 0;
    if (!m_dumpOnDev) {
        onDumpStart();
    }
}

void MainWindow::onLoadWav()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Test WAV"), QString(),
        QStringLiteral("WAV (*.wav);;All (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (m_anList != nullptr) {
        m_anList->clear();
    }
    QMetaObject::invokeMethod(m_io, "startInject", Qt::QueuedConnection,
                              Q_ARG(QString, path));
}

void MainWindow::onDumpActive(bool on)
{
    m_dumpOnDev = on;
    if (!on) {
        closeWav();
    }
    updateWorkUi();
}

void MainWindow::onInjectActive(bool on)
{
    Q_UNUSED(on);
    updateWorkUi();
}

void MainWindow::onInjectStats(unsigned ax25, unsigned ok, unsigned fail)
{
    if (m_analysis != nullptr) {
        m_analysis->setText(QStringLiteral("AX.25 %1   FCS OK %2   fail %3")
                                .arg(ax25)
                                .arg(ok)
                                .arg(fail));
    }
}

void MainWindow::onAnalysisLine(const QString &line)
{
    appendCapped(m_anList, line);
}

void MainWindow::feedFft(const int16_t *s, int n)
{
    static const int kN = 16384;
    static const int kFs = 48000;
    static const int kFmax = 4000;
    const int hop = (m_fftHop >= 1) ? m_fftHop : 8192;
    const int nDisp = kFmax * kN / kFs + 1;
    int i;

    for (i = 0; i < n; i++) {
        m_acc.append(static_cast<float>(s[i]));
    }
    while (m_acc.size() >= kN) {
        QVector<float> re(kN);
        QVector<float> im(kN);
        QVector<float> db(nDisp);
        for (i = 0; i < kN; i++) {
            const float w = 0.5f - 0.5f * std::cos(
                2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                static_cast<float>(kN - 1));
            re[i] = m_acc[i] * w;
            im[i] = 0.0f;
        }
        fft_radix2(re.data(), im.data(), kN);
        for (i = 0; i < nDisp; i++) {
            /* Hann coherent gain 0.5; real sine peak bin ≈ A·N/8. */
            const float mag = 8.0f * std::hypot(re[i], im[i]) /
                              (static_cast<float>(kN) * 32768.0f);
            db[i] = 20.0f * std::log10(mag + 1.0e-12f);
        }
        /* Tune: noise after (fft - mean) lands near this dB. colorOf(-54) ≈ blue. */
        static const float kNormAddDb = -54.0f;
        static const int kNormWinSamp = 15 * kFs;
        if (m_normOn && !m_normReady) {
            if (m_normSum.size() != nDisp) {
                m_normSum.fill(0.0, nDisp);
                m_normSamp = 0;
                m_normFrames = 0;
            }
            for (i = 0; i < nDisp; i++) {
                m_normSum[i] += static_cast<double>(db[i]);
            }
            m_normSamp += hop;
            m_normFrames++;
            if (m_normFrames == 1) {
                setNormStatus(true);
            }
            if (m_normSamp >= kNormWinSamp && m_normFrames > 0) {
                m_normMean.resize(nDisp);
                for (i = 0; i < nDisp; i++) {
                    m_normMean[i] = static_cast<float>(
                        m_normSum[i] / static_cast<double>(m_normFrames));
                }
                m_normReady = true;
                setNormStatus(false);
            }
        }
        if (m_normOn && m_normReady && m_normMean.size() == nDisp) {
            for (i = 0; i < nDisp; i++) {
                db[i] = db[i] - m_normMean[i] + kNormAddDb;
            }
        }
        if (m_fall != nullptr) {
            m_fall->addLine(db.constData(), nDisp);
        }
        m_acc.remove(0, hop);
    }
}

void MainWindow::onPcmChunk(const QByteArray &le16)
{
    const int n = le16.size() / 2;
    const auto *s = reinterpret_cast<const int16_t *>(le16.constData());
    double acc = 0.0;
    float peak = 0.0f;
    int i;

    if (n <= 0) {
        return;
    }
    for (i = 0; i < n; i++) {
        const float a = std::fabs(static_cast<float>(s[i]));
        if (a > peak) {
            peak = a;
        }
        acc += static_cast<double>(s[i]) * static_cast<double>(s[i]);
        if (s[i] == 32767 || s[i] == -32768) {
            m_clips++;
        }
    }
    m_winSq += acc;
    m_winN += n;
    if (peak > m_winPeak) {
        m_winPeak = peak;
    }
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_winN > 0 && now - m_lastBarMs >= 200) {
            const float rms = static_cast<float>(std::sqrt(m_winSq / m_winN));
            const float rmsDb = 20.0f * std::log10(rms / 32768.0f + 1.0e-12f);
            const float peakDb =
                20.0f * std::log10(m_winPeak / 32768.0f + 1.0e-12f);
            if (m_level != nullptr) {
                m_level->setLevels(rmsDb, peakDb, m_clips);
            }
            m_lastBarMs = now;
            if (now - m_lastRmsMs >= 500) {
                m_rms->setText(QStringLiteral("%1 dBFS").arg(rmsDb, 0, 'f', 1));
                m_lastRmsMs = now;
            }
            m_winSq = 0.0;
            m_winN = 0;
            m_winPeak = 0.0f;
        }
    }
    if (m_wav != nullptr) {
        m_wav->write(le16);
        m_wavN += static_cast<quint32>(n);
    }
    feedFft(s, n);
}

static QString callText(const QByteArray &info)
{
    QByteArray t = info;
    for (int i = 0; i < t.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        if (c < 32 || c > 126) {
            t[i] = '.';
        }
    }
    return QString::fromLatin1(t);
}

QFont MainWindow::callMonoFont()
{
    const QStringList want = QStringList()
        << QStringLiteral("JetBrains Mono")
        << QStringLiteral("DejaVu Sans Mono")
        << QStringLiteral("Noto Sans Mono")
        << QStringLiteral("Monospace");
    const QStringList have = QFontDatabase().families();
    for (const QString &name : want) {
        if (have.contains(name)) {
            QFont f(name);
            f.setStyleHint(QFont::Monospace);
            return f;
        }
    }
    QFont f;
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return f;
}

QColor MainWindow::callPendingColor() const
{
    return QColor(0x8B, 0x00, 0x00);
}

QColor MainWindow::callDoneColor() const
{
    return (m_theme == 0) ? QColor(Qt::black) : QColor(230, 230, 230);
}

QTextBlock MainWindow::appendCall(const QString &line, const QColor &color)
{
    if (m_callLog == nullptr) {
        return QTextBlock();
    }
    QTextCharFormat fmt;
    fmt.setForeground(color);
    QTextCursor cur = m_callLog->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(line, fmt);
    const QTextBlock block = cur.block();
    cur.insertText(QStringLiteral("\n"), fmt);
    m_callLog->setTextCursor(cur);
    m_callLog->ensureCursorVisible();
    return block;
}

void MainWindow::recolorCallBlock(const QTextBlock &block, const QColor &color)
{
    if (m_callLog == nullptr || !block.isValid()) {
        return;
    }
    QTextCursor cur(block);
    cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    QTextCharFormat fmt;
    fmt.setForeground(color);
    cur.mergeCharFormat(fmt);
}

void MainWindow::startCall(bool listen)
{
    if (!m_devUp || m_callArmed) {
        return;
    }
    QMetaObject::invokeMethod(
        m_io, "startCall", Qt::QueuedConnection,
        Q_ARG(QString, m_callSrc->text()),
        Q_ARG(QString, m_callDst->text()),
        Q_ARG(QString, m_callPath->text()),
        Q_ARG(bool, listen),
        Q_ARG(int, m_t1->value()),
        Q_ARG(int, m_t2->value()),
        Q_ARG(int, m_t3->value()),
        Q_ARG(int, m_n2->value()),
        Q_ARG(int, m_paclen->value()),
        Q_ARG(int, m_maxframe->value()),
        Q_ARG(int, m_txdelay->value()),
        Q_ARG(int, m_txtail->value()));
}

void MainWindow::flushCallOut()
{
    if (m_callSt != GT_LINK_CONNECTED) {
        return;
    }
    for (const CallOut &o : m_callOut) {
        if (o.st != 0) {
            continue;
        }
        if (o.bytes.size() > m_paclen->value()) {
            m_status->setText(QStringLiteral("Line longer than paclen."));
            continue;
        }
        QMetaObject::invokeMethod(m_io, "queueCallLine", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, o.bytes));
        return;
    }
}

void MainWindow::onCallConnect()
{
    startCall(false);
}

void MainWindow::onCallListen()
{
    startCall(true);
}

void MainWindow::onCallHang()
{
    QMetaObject::invokeMethod(m_io, "stopCall", Qt::QueuedConnection);
}

void MainWindow::onCallSend()
{
    const QByteArray line = m_callLine->text().toLatin1();
    CallOut o;

    m_callLine->clear();
    o.block = appendCall(callText(line), callPendingColor());
    o.bytes = line;
    o.st = 0;
    m_callOut.append(o);
    flushCallOut();
}

void MainWindow::onCallArmed(bool on)
{
    m_callArmed = on;
    if (!on) {
        m_callListening = false;
        m_callSt = GT_LINK_IDLE;
        if (m_callState != nullptr) {
            m_callState->setText(QStringLiteral("Idle"));
        }
    }
    updateWorkUi();
}

void MainWindow::onCallState(int state, const QString &peer, bool listen,
                             int tries, int n2)
{
    const int t = (tries > 0) ? tries : 1;
    const int m = (n2 > 0) ? n2 : 10;
    const QString pm = QStringLiteral("[%1/%2]").arg(t).arg(m);
    QString text;

    m_callSt = state;
    m_callListening = listen;
    m_callTries = tries;
    m_callN2 = n2;
    if (state == GT_LINK_IDLE) {
        if (m_callArmed && listen && !peer.isEmpty()) {
            const int cut = peer.indexOf(QLatin1Char('>'));
            if (cut > 0) {
                text = QStringLiteral("Listening %1 from %2")
                           .arg(peer.left(cut), peer.mid(cut + 1));
            } else {
                text = QStringLiteral("Listening %1").arg(peer);
            }
        } else {
            text = QStringLiteral("Idle");
        }
    } else if (state == GT_LINK_SABM) {
        text = QStringLiteral("Sending SABM %1 %2").arg(peer, pm);
    } else if (state == GT_LINK_CONNECTED) {
        if (tries > 0) {
            text = QStringLiteral("Retransmitting %1 %2").arg(peer, pm);
        } else {
            text = QStringLiteral("Connected %1").arg(peer);
        }
    } else if (state == GT_LINK_DISC) {
        text = QStringLiteral("Sending DISC %1 %2").arg(peer, pm);
    } else if (state == GT_LINK_DEAD) {
        text = QStringLiteral("Dead");
    } else {
        text = QStringLiteral("?");
    }
    if (m_callState != nullptr) {
        m_callState->setText(text);
    }
    updateWorkUi();
}

void MainWindow::onCallUp(const QString &src, const QString &dst)
{
    appendCall(QStringLiteral("* UP %1>%2").arg(src, dst),
               QColor(140, 190, 140));
    flushCallOut();
}

void MainWindow::onCallDown()
{
    appendCall(QStringLiteral("* DOWN"), QColor(190, 140, 140));
}

void MainWindow::onCallInfo(const QByteArray &info)
{
    appendCall(callText(info), callDoneColor());
}

void MainWindow::onCallUi(const QByteArray &info)
{
    appendCall(QStringLiteral("[UI] %1").arg(callText(info)),
               QColor(230, 170, 70));
}

void MainWindow::onCallQueued(const QByteArray &line)
{
    for (CallOut &o : m_callOut) {
        if (o.st == 0 && o.bytes == line) {
            o.st = 1;
            break;
        }
    }
    flushCallOut();
}

void MainWindow::onCallAcked(int n)
{
    int left = n;

    for (CallOut &o : m_callOut) {
        if (left <= 0) {
            break;
        }
        if (o.st != 1) {
            continue;
        }
        o.st = 2;
        recolorCallBlock(o.block, callDoneColor());
        left--;
    }
    flushCallOut();
}

void MainWindow::onCallError(const QString &text)
{
    m_status->setText(text);
}
