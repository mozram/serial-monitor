#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStatusBar>
#include <QScrollBar>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QLineEdit>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    initUi();

    m_receiveTimer = new QTimer(this);
    m_receiveTimer->setSingleShot(true);
    connect(m_receiveTimer, &QTimer::timeout, this, &MainWindow::flushReceiveBuffer);

    m_serialThread = new QThread(this);
    m_worker = new SerialWorker();
    m_worker->moveToThread(m_serialThread);

    connect(m_serialThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &MainWindow::destroyed, m_serialThread, &QThread::quit);

    connect(m_worker, &SerialWorker::dataReceived, this, &MainWindow::handleDataReceived);
    connect(m_worker, &SerialWorker::errorOccurred, this, &MainWindow::handleError);
    connect(m_worker, &SerialWorker::connectionStatusChanged, this, &MainWindow::updateConnectionStatus);

    m_serialThread->start();
    refreshPorts();
}

MainWindow::~MainWindow() {
    m_serialThread->quit();
    m_serialThread->wait();
}

void MainWindow::initUi() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // Connection Group
    QGroupBox *connGroup = new QGroupBox("Connection Settings", this);
    QHBoxLayout *connLayout = new QHBoxLayout(connGroup);

    m_portCombo = new QComboBox(this);
    QPushButton *refreshBtn = new QPushButton("Refresh", this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);

    m_baudCombo = new QComboBox(this);
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");

    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    connLayout->addWidget(new QLabel("Port:"));
    connLayout->addWidget(m_portCombo);
    connLayout->addWidget(refreshBtn);
    connLayout->addWidget(new QLabel("Baud:"));
    connLayout->addWidget(m_baudCombo);
    connLayout->addStretch();
    connLayout->addWidget(m_connectBtn);
    mainLayout->addWidget(connGroup);

    // Terminal Options
    QHBoxLayout *optLayout = new QHBoxLayout();
    m_hexCheck = new QCheckBox("Hex", this);
    m_timeCheck = new QCheckBox("Time", this);
    m_timeCheck->setChecked(false);
    m_sepCheck = new QCheckBox("Separate", this);
    m_metaCheck = new QCheckBox("Meta", this);
    m_metaCheck->setChecked(false);

    m_hysteresisSpin = new QSpinBox(this);
    m_hysteresisSpin->setRange(0, 5000);
    m_hysteresisSpin->setValue(20);
    m_hysteresisSpin->setSuffix(" ms");

    optLayout->addWidget(m_hexCheck);
    optLayout->addWidget(m_timeCheck);
    optLayout->addWidget(m_sepCheck);
    optLayout->addWidget(m_metaCheck);
    optLayout->addSpacing(20);
    optLayout->addWidget(new QLabel("Hysteresis:"));
    optLayout->addWidget(m_hysteresisSpin);
    optLayout->addSpacing(20);
    optLayout->addWidget(new QLabel("Decoder:"));
    m_decoderCombo = new QComboBox(this);
    m_decoderCombo->addItem("None");
    m_decoderCombo->addItems(PluginManager::instance().pluginNames());
    optLayout->addWidget(m_decoderCombo);
    optLayout->addStretch();
    mainLayout->addLayout(optLayout);
    
    connect(m_decoderCombo, &QComboBox::currentIndexChanged, this, &MainWindow::rebuildTerminal);

    connect(m_hexCheck, &QCheckBox::toggled, this, &MainWindow::rebuildTerminal);
    connect(m_timeCheck, &QCheckBox::toggled, this, &MainWindow::rebuildTerminal);
    connect(m_sepCheck, &QCheckBox::toggled, [this](){
        m_timestampDisplay->setVisible(m_sepCheck->isChecked());
        rebuildTerminal();
    });
    connect(m_metaCheck, &QCheckBox::toggled, this, &MainWindow::rebuildTerminal);

    // Terminal Display
    QHBoxLayout *termLayout = new QHBoxLayout();
    termLayout->setSpacing(0);
    termLayout->setContentsMargins(0, 0, 0, 0);

    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);

    m_timestampDisplay = new QPlainTextEdit(this);
    m_timestampDisplay->setReadOnly(true);
    m_timestampDisplay->setFont(monoFont);
    m_timestampDisplay->setFixedWidth(110);
    m_timestampDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_timestampDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_timestampDisplay->setFrameStyle(QFrame::NoFrame);
    m_timestampDisplay->document()->setDocumentMargin(0);
    m_timestampDisplay->viewport()->setContentsMargins(0, 0, 0, 0);
    m_timestampDisplay->hide();

    m_terminalDisplay = new QPlainTextEdit(this);
    m_terminalDisplay->setReadOnly(true);
    m_terminalDisplay->setFont(monoFont);
    m_terminalDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_terminalDisplay->setFrameStyle(QFrame::NoFrame);
    m_terminalDisplay->document()->setDocumentMargin(0);
    m_terminalDisplay->viewport()->setContentsMargins(0, 0, 0, 0);

    termLayout->addWidget(m_timestampDisplay);
    termLayout->addWidget(m_terminalDisplay);
    mainLayout->addLayout(termLayout);

    connect(m_terminalDisplay->verticalScrollBar(), &QScrollBar::valueChanged,
            m_timestampDisplay->verticalScrollBar(), &QScrollBar::setValue);

    // Send Data
    QGroupBox *sendGroup = new QGroupBox("Send Data", this);
    QVBoxLayout *sendGroupLayout = new QVBoxLayout(sendGroup);

    QHBoxLayout *sendModeLayout = new QHBoxLayout();
    m_sendTextRadio = new QRadioButton("Text", this);
    m_sendHexRadio = new QRadioButton("Hex (space-separated)", this);
    m_sendTextRadio->setChecked(true);
    sendModeLayout->addWidget(new QLabel("Send Mode:"));
    sendModeLayout->addWidget(m_sendTextRadio);
    sendModeLayout->addWidget(m_sendHexRadio);
    sendModeLayout->addStretch();
    sendGroupLayout->addLayout(sendModeLayout);

    QHBoxLayout *sendInputLayout = new QHBoxLayout();
    m_sendInput = new QLineEdit(this);
    m_sendInput->setPlaceholderText("Enter text to send... (or hex: FF AA 00 for hex mode)");
    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setEnabled(false);
    m_newlineCheck = new QCheckBox("Add Newline", this);
    m_newlineCheck->setChecked(true);

    sendInputLayout->addWidget(m_sendInput);
    sendInputLayout->addWidget(m_newlineCheck);
    sendInputLayout->addWidget(m_sendBtn);
    sendGroupLayout->addLayout(sendInputLayout);
    mainLayout->addWidget(sendGroup);

    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::sendData);
    connect(m_sendInput, &QLineEdit::returnPressed, this, &MainWindow::sendData);

    // Action Buttons
    QHBoxLayout *actLayout = new QHBoxLayout();
    QPushButton *clearBtn = new QPushButton("Clear Terminal", this);
    QPushButton *exportBtn = new QPushButton("Export to File", this);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearTerminal);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportLogs);
    actLayout->addWidget(clearBtn);
    actLayout->addWidget(exportBtn);
    mainLayout->addLayout(actLayout);

    setWindowTitle("Serial Terminal");
    resize(900, 700);
}

void MainWindow::refreshPorts() {
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString name = info.portName();
        if (name.startsWith("ttyS")) continue;

        QString desc = info.description();
        QString item = name;
        if (!desc.isEmpty()) item += " - " + desc;
        m_portCombo->addItem(item, name);
    }
}

void MainWindow::toggleConnection() {
    if (m_isConnected) {
        QMetaObject::invokeMethod(m_worker, "disconnectPort");
    } else {
        QString port = m_portCombo->currentData().toString();
        int baud = m_baudCombo->currentText().toInt();
        QMetaObject::invokeMethod(m_worker, "connectPort",
                                 Q_ARG(QString, port), Q_ARG(int, baud),
                                 Q_ARG(int, 8), Q_ARG(int, 1), Q_ARG(int, 0));
    }
}

void MainWindow::updateConnectionStatus(bool connected, const QString &msg) {
    m_isConnected = connected;
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    m_connectBtn->setStyleSheet(connected ? "background-color: #f44336; color: white; font-weight: bold;" :
                                           "background-color: #4CAF50; color: white; font-weight: bold;");
    m_sendBtn->setEnabled(connected);
    statusBar()->showMessage(msg);
    logEvent("INFO", msg.toUtf8(), "SYSTEM");
}

void MainWindow::handleDataReceived(const QByteArray &data) {
    m_receiveBuffer.append(data);
    if (!m_receiveTimer->isActive()) {
        m_receiveTimer->start(m_hysteresisSpin->value());
    }
}

void MainWindow::flushReceiveBuffer() {
    if (m_receiveBuffer.isEmpty()) return;
    logEvent("RX", m_receiveBuffer, "RX");
    m_receiveBuffer.clear();
}

void MainWindow::sendData() {
    QString text = m_sendInput->text();
    if (text.isEmpty()) return;

    QByteArray data;
    QString prefix = "TX";

    if (m_sendHexRadio->isChecked()) {
        QString clean = text.toUpper();
        clean.remove(QRegularExpression("[^0-9A-F]"));
        if (clean.length() % 2 != 0) {
            QMessageBox::warning(this, "Format Error", "Hex string must have an even number of characters.");
            return;
        }
        data = QByteArray::fromHex(clean.toUtf8());
        if (data.isEmpty()) {
            QMessageBox::warning(this, "Format Error", "Invalid hex format.");
            return;
        }
        prefix = "TX HEX";
    } else {
        data = text.toUtf8();
        if (m_newlineCheck->isChecked()) data.append('\n');
    }

    QMetaObject::invokeMethod(m_worker, "writeData", Q_ARG(QByteArray, data));
    logEvent("TX", data, prefix);
    m_sendInput->clear();
}

void MainWindow::logEvent(const QString &type, const QByteArray &data, const QString &prefix) {
    LogEntry entry = { QDateTime::currentDateTime(), type, data, prefix };
    m_logEntries.append(entry);
    if (m_logEntries.size() > 5000) m_logEntries.removeFirst();
    renderEntry(entry);
}

void MainWindow::renderEntry(const LogEntry &entry, bool scroll) {
    QString dataText;
    if (entry.type == "INFO") {
        dataText = QString::fromUtf8(entry.data);
    } else if (m_hexCheck->isChecked()) {
        dataText = entry.data.toHex(' ').toUpper() + "\n";
    } else {
        QString currentDecoder = m_decoderCombo->currentText();
        if (currentDecoder != "None") {
            dataText = PluginManager::instance().decode(currentDecoder, entry.data);
        } else {
            dataText = QString::fromUtf8(entry.data).replace("\r\n", "\n").replace('\r', '\n');
        }
    }
    if (!dataText.endsWith('\n')) dataText.append('\n');

    QString prefixStr = entry.prefix;
    if ((entry.type == "RX" || entry.type == "TX") && m_hexCheck->isChecked()) prefixStr += " HEX";
    QString finalPrefix = m_metaCheck->isChecked() ? QString("[%1] ").arg(prefixStr) : "";

    QString timestamp = m_timeCheck->isChecked() ? entry.time.toString("hh:mm:ss.zzz") : "";

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(13, 4);
    blockFormat.setTopMargin(0);
    blockFormat.setBottomMargin(0);

    QTextCursor termCursor(m_terminalDisplay->document());
    termCursor.movePosition(QTextCursor::End);

    QTextCursor tsCursor(m_timestampDisplay->document());
    tsCursor.movePosition(QTextCursor::End);

    // Smart auto-scroll check
    QScrollBar *vBar = m_terminalDisplay->verticalScrollBar();
    bool atBottom = vBar->value() >= (vBar->maximum() - 10);

    QStringList lines = dataText.split('\n');
    if (dataText.endsWith('\n')) lines.removeLast();

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];

        // 1. Terminal Panel Insertion
        if (!termCursor.atStart()) termCursor.insertBlock(blockFormat);
        else termCursor.setBlockFormat(blockFormat);

        QString inlineTs = "";
        if (!m_sepCheck->isChecked() && !timestamp.isEmpty()) {
            inlineTs = (i == 0 ? timestamp + " " : QString(timestamp.length() + 1, ' '));
        }

        QString linePrefix = (i == 0 ? finalPrefix : QString(finalPrefix.length(), ' '));
        termCursor.insertText(inlineTs + linePrefix + line);

        // 2. Timestamp Panel Insertion
        if (m_sepCheck->isChecked() && m_timeCheck->isChecked()) {
            if (!tsCursor.atStart()) tsCursor.insertBlock(blockFormat);
            else tsCursor.setBlockFormat(blockFormat);

            if (i == 0) tsCursor.insertText(timestamp);
            else tsCursor.insertText(QString(timestamp.length() - 1, ' ') + "|");
        }
    }

    if (scroll && atBottom) {
        m_terminalDisplay->ensureCursorVisible();
        m_timestampDisplay->ensureCursorVisible();
        vBar->setValue(vBar->maximum());
    }
}

void MainWindow::rebuildTerminal() {
    m_terminalDisplay->setUpdatesEnabled(false);
    m_timestampDisplay->setUpdatesEnabled(false);
    m_terminalDisplay->clear();
    m_timestampDisplay->clear();
    for (const auto &entry : m_logEntries) renderEntry(entry, false);
    m_terminalDisplay->setUpdatesEnabled(true);
    m_timestampDisplay->setUpdatesEnabled(true);
    m_terminalDisplay->verticalScrollBar()->setValue(m_terminalDisplay->verticalScrollBar()->maximum());
}

void MainWindow::clearTerminal() {
    m_logEntries.clear();
    rebuildTerminal();
}

void MainWindow::exportLogs() {
    QString path = QFileDialog::getSaveFileName(this, "Export Logs",
                   QStandardPaths::writableLocation(QStandardPaths::HomeLocation), "Text Files (*.txt)");
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_terminalDisplay->toPlainText().toUtf8());
        }
    }
}

void MainWindow::handleError(const QString &error) {
    QMessageBox::critical(this, "Error", error);
}
