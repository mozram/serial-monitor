#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QThread>
#include <QHash>
#include "SerialWorker.h"
#include "PluginManager.h"

struct LogEntry {
    QString type; // RX, TX, INFO
    QByteArray data;
    bool streaming = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshPorts();
    void toggleConnection();
    void handleDataReceived(const QByteArray &data);
    void handleError(const QString &error);
    void updateConnectionStatus(bool connected, const QString &msg);
    void sendData();
    void flushReceiveBuffer();
    void flushLiveRenderBuffer();
    void rebuildTerminal();
    void rebuildTerminalChunk();
    void exportLogs();
    void clearTerminal();

private:
    void initUi();
    void logEvent(const QString &type, const QByteArray &data, bool streaming = false);
    void renderEntry(const LogEntry &entry, bool scroll = true);

    // UI Components
    QComboBox *m_portCombo, *m_baudCombo, *m_dataBitsCombo, *m_stopBitsCombo, *m_parityCombo;
    QPushButton *m_connectBtn, *m_sendBtn;
    QPlainTextEdit *m_terminalDisplay;
    QCheckBox *m_hexCheck, *m_newlineCheck;
    QCheckBox *m_hysteresisEnabledCheck;
    QRadioButton *m_sendTextRadio, *m_sendHexRadio;
    QSpinBox *m_hysteresisSpin;
    QComboBox *m_decoderCombo;
    QComboBox *m_sendInput;

    // Backend
    QThread *m_serialThread;
    SerialWorker *m_worker;
    QList<LogEntry> m_logEntries;
    QByteArray m_receiveBuffer;
    QByteArray m_liveRenderBuffer;
    QTimer *m_receiveTimer;
    QTimer *m_liveRenderTimer;
    QTimer *m_rebuildTimer;
    QList<LogEntry> m_rebuildEntries;
    QList<LogEntry> m_liveEntriesDuringRebuild;
    QHash<QString, QHash<QByteArray, QString>> m_decoderCache;
    int m_rebuildIndex = 0;
    bool m_rebuilding = false;
    bool m_terminalLineOpen = false;
    bool m_isConnected = false;
};

#endif // MAINWINDOW_H
