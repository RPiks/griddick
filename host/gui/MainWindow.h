#ifndef MAINWINDOW_H
#define MAINWINDOW_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * MainWindow.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <QMainWindow>
#include <QList>
#include <QTextBlock>
#include <QVector>

class QAction;
class QByteArray;
class QCheckBox;
class QColor;
class QComboBox;
class QFile;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;
class QTimer;
class IoThread;
class LevelBar;
class Waterfall;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshDevices();
    void onConnect();
    void onDisconnect();
    void onAbout();
    void onIoStatus(const QString &text);
    void onConnected(const QString &fwVersion);
    void onDisconnected();
    void onLogLine(const QString &line);
    void onHeardFrame(const QString &line);
    void onCfgJson(const QString &json);
    void onTimeAck(quint64 hostMs, quint64 tncMs);
    void onRms(unsigned value);
    void onCfgGet();
    void onCfgPut();
    void onTimeSync();
    void onTimeGet();
    void setTheme(int id);
    void onDumpStart();
    void onDumpStop();
    void onSaveWav();
    void onLoadWav();
    void onPcmChunk(const QByteArray &le16);
    void onDumpActive(bool on);
    void onInjectActive(bool on);
    void onInjectStats(unsigned ax25, unsigned ok, unsigned fail);
    void onAnalysisLine(const QString &line);
    void onFlashBrowse();
    void onFlashScan();
    void onFlash();
    void onTxSend();
    void onTxBeacon();
    void onTxStop();
    void onTxModeChanged();
    void onTxFileBrowse();
    void onCallConnect();
    void onCallListen();
    void onCallHang();
    void onCallSend();
    void onCallArmed(bool on);
    void onCallState(int state, const QString &peer, bool listen, int tries,
                     int n2);
    void onCallUp(const QString &src, const QString &dst);
    void onCallDown();
    void onCallInfo(const QByteArray &info);
    void onCallUi(const QByteArray &info);
    void onCallQueued(const QByteArray &line);
    void onCallAcked(int n);
    void onCallError(const QString &text);
    void onAprsSend();
    void onAprsBeacon();
    void onAprsStop();
    void onAprsHeard(const QString &line);

private:
    QWidget *buildTestTab();
    QWidget *buildWaterfallTab();
    void onBinsPerPixel(int index);
    void onFallSpeed(int index);
    void onStartHz(int hz);
    void onTimeMarks(bool on);
    void onNormalize(bool on);
    void resetNorm();
    void setNormStatus(bool accumulating);
    QWidget *buildMonitorTab();
    QWidget *buildSettingsTab();
    QWidget *buildTxTab();
    QWidget *buildCallTab();
    QWidget *buildFlashTab();
    QWidget *buildReservedTab(const QString &name);
    QWidget *buildAprsTab();
    bool buildTxFrame(QByteArray *out, QString *err) const;
    bool buildAprsFrame(QByteArray *out, QString *err) const;
    void updateWorkUi();
    QTextBlock appendCall(const QString &line, const QColor &color);
    void startCall(bool listen);
    void flushCallOut();
    void recolorCallBlock(const QTextBlock &block, const QColor &color);
    QColor callPendingColor() const;
    QColor callDoneColor() const;
    static QFont callMonoFont();

    void setLed(bool on);
    void setConnectedUi(bool on);
    void appendCapped(QListWidget *list, const QString &line);
    void applyCfgToForm(const struct gt_cfg *c);
    bool formToCfg(struct gt_cfg *c) const;
    void loadUiSettings();
    void saveUiSettings();
    void applyTheme();
    void setActionStatus(const QString &text);
    static QString fmtClock(quint64 ms);
    void closeWav();
    void feedFft(const int16_t *s, int n);
    QString findRp2Volume() const;
    QString findDefaultUf2() const;
    void updateFlashReady();

    QComboBox *m_dev;
    QAction *m_actDark;
    QAction *m_actLight;
    QAction *m_actNight;
    QPushButton *m_connect;
    QPushButton *m_disconnect;
    QLabel *m_led;
    QLabel *m_fwVer;
    QLabel *m_appVer;
    QLabel *m_rms;
    QLabel *m_status;
    QString m_statusHold;
    QListWidget *m_log;
    QListWidget *m_heard;
    QCheckBox *m_legacy;
    QLabel *m_build;
    QSpinBox *m_txdelay;
    QSpinBox *m_persist;
    QSpinBox *m_slottime;
    QSpinBox *m_txtail;
    QCheckBox *m_fulldup;
    QComboBox *m_linkmode;
    QLineEdit *m_mycall;
    QSpinBox *m_paclen;
    QSpinBox *m_maxframe;
    QSpinBox *m_t1;
    QSpinBox *m_t2;
    QSpinBox *m_t3;
    QSpinBox *m_n2;
    QLabel *m_clock;
    QWidget *m_settingsBox;
    QPushButton *m_dumpOn;
    QPushButton *m_dumpOff;
    QPushButton *m_saveWav;
    QPushButton *m_loadWav;
    LevelBar *m_level;
    Waterfall *m_fall;
    QComboBox *m_bpp;
    QComboBox *m_speed;
    QSpinBox *m_startHz;
    QCheckBox *m_timeMarks;
    QCheckBox *m_normalize;
    QLabel *m_analysis;
    QListWidget *m_anList;
    QLineEdit *m_uf2;
    QLabel *m_rp2;
    QPushButton *m_flash;
    QLineEdit *m_txSrc;
    QLineEdit *m_txDst;
    QLineEdit *m_txPath;
    QSpinBox *m_txCtrl;
    QSpinBox *m_txPid;
    QComboBox *m_txMode;
    QPlainTextEdit *m_txBody;
    QLineEdit *m_txFile;
    QPushButton *m_txBrowse;
    QPushButton *m_txSend;
    QPushButton *m_txBeacon;
    QPushButton *m_txStop;
    QSpinBox *m_txEvery;
    QSpinBox *m_txCount;
    QTimer *m_txTimer;
    int m_txLeft;
    QWidget *m_txBox;
    QWidget *m_aprsBox;
    QLineEdit *m_aprsLat;
    QLineEdit *m_aprsLon;
    QLineEdit *m_aprsCmt;
    QLineEdit *m_aprsStatus;
    QLineEdit *m_aprsTo;
    QLineEdit *m_aprsMsg;
    QLineEdit *m_aprsDst;
    QLineEdit *m_aprsPath;
    QPushButton *m_aprsSend;
    QPushButton *m_aprsBeacon;
    QPushButton *m_aprsStop;
    QSpinBox *m_aprsEvery;
    QSpinBox *m_aprsCount;
    QTimer *m_aprsTimer;
    int m_aprsLeft;
    QListWidget *m_aprsLog;
    QWidget *m_callBox;
    QLineEdit *m_callSrc;
    QLineEdit *m_callDst;
    QLineEdit *m_callPath;
    QPushButton *m_callListen;
    QPushButton *m_callGo;
    QPushButton *m_callHang;
    QLabel *m_callState;
    QPlainTextEdit *m_callLog;
    QLineEdit *m_callLine;
    QPushButton *m_callSend;
    bool m_devUp;
    bool m_callArmed;
    bool m_callListening;
    int m_callSt;
    int m_callTries;
    int m_callN2;
    struct CallOut {
        QTextBlock block;
        QByteArray bytes;
        int st;
    };
    QList<CallOut> m_callOut;
    QThread *m_thread;
    IoThread *m_io;
    int m_theme;
    int m_statOp;
    bool m_dumpOnDev;
    qint64 m_lastBarMs;
    qint64 m_lastRmsMs;
    double m_winSq;
    int m_winN;
    float m_winPeak;
    QFile *m_wav;
    quint32 m_wavN;
    QVector<float> m_acc;
    int m_fftHop;
    bool m_normOn;
    bool m_normReady;
    int m_normSamp;
    int m_normFrames;
    QVector<double> m_normSum;
    QVector<float> m_normMean;
    int m_clips;
};

#endif
