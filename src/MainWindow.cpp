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
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextOption>
#include <QBrush>
#include <QColor>
#include <QLineEdit>
#include <QPalette>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    initUi();

    m_receiveTimer = new QTimer(this);
    m_receiveTimer->setSingleShot(true);
    connect(m_receiveTimer, &QTimer::timeout, this, &MainWindow::flushReceiveBuffer);

    m_liveRenderTimer = new QTimer(this);
    m_liveRenderTimer->setSingleShot(true);
    connect(m_liveRenderTimer, &QTimer::timeout, this, &MainWindow::flushLiveRenderBuffer);

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    connect(m_rebuildTimer, &QTimer::timeout, this, &MainWindow::rebuildTerminalChunk);

    connect(m_hysteresisEnabledCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_hysteresisSpin->setEnabled(enabled);
        m_receiveTimer->stop();
        m_liveRenderTimer->stop();
        flushReceiveBuffer();
        flushLiveRenderBuffer();
        rebuildTerminal();
    });

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
    connGroup->setToolTip("Configure the serial port and connection speed.");
    QHBoxLayout *connLayout = new QHBoxLayout(connGroup);

    m_portCombo = new QComboBox(this);
    m_portCombo->setToolTip("Select the serial port to connect to.");
    QPushButton *refreshBtn = new QPushButton("Refresh", this);
    refreshBtn->setToolTip("Refresh the list of available serial ports.");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);

    m_baudCombo = new QComboBox(this);
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");
    m_baudCombo->setToolTip("Select the baud rate used by the serial connection.");

    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    m_connectBtn->setToolTip("Open or close the serial connection.");
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    QLabel *portLabel = new QLabel("Port:", this);
    portLabel->setToolTip("Serial device used for communication.");
    connLayout->addWidget(portLabel);
    connLayout->addWidget(m_portCombo);
    connLayout->addWidget(refreshBtn);
    QLabel *baudLabel = new QLabel("Baud:", this);
    baudLabel->setToolTip("Number of data symbols transmitted per second.");
    connLayout->addWidget(baudLabel);
    connLayout->addWidget(m_baudCombo);
    connLayout->addStretch();
    connLayout->addWidget(m_connectBtn);
    mainLayout->addWidget(connGroup);

    // Terminal Options
    QHBoxLayout *optLayout = new QHBoxLayout();
    m_hexCheck = new QCheckBox("Hex", this);
    m_hexCheck->setToolTip("Display received and sent data in hexadecimal.");

    m_hysteresisEnabledCheck = new QCheckBox("Enable", this);
    m_hysteresisEnabledCheck->setChecked(false);
    m_hysteresisEnabledCheck->setToolTip(
        "Group incoming serial chunks received within the same delay.");
    m_hysteresisSpin = new QSpinBox(this);
    m_hysteresisSpin->setRange(0, 5000);
    m_hysteresisSpin->setValue(20);
    m_hysteresisSpin->setSuffix(" ms");
    m_hysteresisSpin->setToolTip(
        "Maximum quiet period in milliseconds before buffered receive data is displayed.");

    const QString hysteresisHelpText =
        "Groups incoming serial chunks into one receive entry after no data arrives "
        "for the selected delay. Disable it to display each received chunk immediately.";
    QLabel *hysteresisLabel = new QLabel("Hysteresis:", this);
    hysteresisLabel->setAccessibleName("About Hysteresis");
    hysteresisLabel->setToolTip(hysteresisHelpText);

    optLayout->addWidget(m_hexCheck);
    optLayout->addSpacing(20);
    optLayout->addWidget(hysteresisLabel);
    optLayout->addWidget(m_hysteresisEnabledCheck);
    optLayout->addWidget(m_hysteresisSpin);
    optLayout->addSpacing(20);
    QLabel *decoderLabel = new QLabel("Decoder:", this);
    decoderLabel->setToolTip(
        "Apply a Python plugin to decode received data in the terminal.");
    optLayout->addWidget(decoderLabel);
    m_decoderCombo = new QComboBox(this);
    m_decoderCombo->addItem("None");
    m_decoderCombo->addItems(PluginManager::instance().pluginNames());
    m_decoderCombo->setToolTip(
        "Select None or a Python decoder plugin from the plugins folder.");
    optLayout->addWidget(m_decoderCombo);
    optLayout->addStretch();
    mainLayout->addLayout(optLayout);
    
    connect(m_decoderCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        m_decoderCombo->hidePopup();
        m_decoderCombo->clearFocus();
        QTimer::singleShot(0, this, [this]() {
            m_decoderCombo->hidePopup();
            rebuildTerminal();
        });
    });

    connect(m_hexCheck, &QCheckBox::toggled, this, &MainWindow::rebuildTerminal);

    // Terminal Display
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);

    m_terminalDisplay = new QPlainTextEdit(this);
    m_terminalDisplay->setReadOnly(true);
    m_terminalDisplay->setToolTip("Received serial data and sent data.");
    m_terminalDisplay->setFont(monoFont);
    m_terminalDisplay->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_terminalDisplay->setWordWrapMode(QTextOption::WrapAnywhere);
    m_terminalDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_terminalDisplay->setFrameStyle(QFrame::NoFrame);
    m_terminalDisplay->document()->setDocumentMargin(0);
    m_terminalDisplay->viewport()->setContentsMargins(0, 0, 0, 0);
    m_terminalDisplay->document()->setMaximumBlockCount(5000);

    mainLayout->addWidget(m_terminalDisplay);

    // Send Data
    QGroupBox *sendGroup = new QGroupBox("Send Data", this);
    sendGroup->setToolTip("Send text or hexadecimal bytes over the serial connection.");
    QVBoxLayout *sendGroupLayout = new QVBoxLayout(sendGroup);

    QHBoxLayout *sendModeLayout = new QHBoxLayout();
    m_sendTextRadio = new QRadioButton("Text", this);
    m_sendHexRadio = new QRadioButton("Hex", this);
    m_sendTextRadio->setChecked(true);
    m_sendTextRadio->setToolTip("Send the input as UTF-8 text.");
    m_sendHexRadio->setToolTip("Send the input as hexadecimal bytes.");
    QLabel *sendModeLabel = new QLabel("Send Mode:", this);
    sendModeLabel->setToolTip("Choose whether the send input is text or hexadecimal.");
    sendModeLayout->addWidget(sendModeLabel);
    sendModeLayout->addWidget(m_sendTextRadio);
    sendModeLayout->addWidget(m_sendHexRadio);
    sendModeLayout->addStretch();
    sendGroupLayout->addLayout(sendModeLayout);

    QHBoxLayout *sendInputLayout = new QHBoxLayout();
    m_sendInput = new QComboBox(this);
    m_sendInput->setEditable(true);
    m_sendInput->setInsertPolicy(QComboBox::NoInsert);
    m_sendInput->lineEdit()->setPlaceholderText("Enter text to send... (or hex: FF AA 00 for hex mode)");
    m_sendInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_sendInput->setToolTip("Enter data to send. Enter FF AA 00 in Hex mode.");

    // Load history
    QFile historyFile("history.txt");
    if (historyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&historyFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (!line.isEmpty()) {
                m_sendInput->addItem(line);
            }
        }
        historyFile.close();
    }
    m_sendInput->setCurrentText(""); // Clear current text

    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setEnabled(false);
    m_sendBtn->setToolTip("Send the entered data over the serial connection.");
    m_newlineCheck = new QCheckBox("Add Newline", this);
    m_newlineCheck->setChecked(true);
    m_newlineCheck->setToolTip("Append a newline character in Text mode.");

    sendInputLayout->addWidget(m_sendInput);
    sendInputLayout->addWidget(m_newlineCheck);
    sendInputLayout->addWidget(m_sendBtn);
    sendGroupLayout->addLayout(sendInputLayout);
    mainLayout->addWidget(sendGroup);

    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::sendData);
    connect(m_sendInput->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::sendData);

    // Action Buttons
    QHBoxLayout *actLayout = new QHBoxLayout();
    QPushButton *clearBtn = new QPushButton("Clear Terminal", this);
    QPushButton *exportBtn = new QPushButton("Export to File", this);
    clearBtn->setToolTip("Clear the terminal display and receive history.");
    exportBtn->setToolTip("Save the terminal display to a text file.");
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
    logEvent("INFO", msg.toUtf8());
}

void MainWindow::handleDataReceived(const QByteArray &data) {
    if (data.isEmpty()) return;
    if (!m_hysteresisEnabledCheck->isChecked()) {
        m_liveRenderBuffer.append(data);
        if (!m_liveRenderTimer->isActive()) m_liveRenderTimer->start(16);
        return;
    }

    m_receiveBuffer.append(data);
    m_receiveTimer->start(m_hysteresisSpin->value());
}

void MainWindow::flushReceiveBuffer() {
    if (m_receiveBuffer.isEmpty()) return;
    logEvent("RX", m_receiveBuffer);
    m_receiveBuffer.clear();
}

void MainWindow::flushLiveRenderBuffer() {
    if (m_liveRenderBuffer.isEmpty()) return;
    logEvent("RX", m_liveRenderBuffer, true);
    m_liveRenderBuffer.clear();
}

void MainWindow::sendData() {
    QString text = m_sendInput->currentText();
    if (text.isEmpty()) return;

    QByteArray data;

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
    } else {
        data = text.toUtf8();
        if (m_newlineCheck->isChecked()) data.append('\n');
    }

    QMetaObject::invokeMethod(m_worker, "writeData", Q_ARG(QByteArray, data));
    logEvent("TX", data);

    // Update history
    int existingIndex = m_sendInput->findText(text);
    if (existingIndex != -1) {
        m_sendInput->removeItem(existingIndex);
    }
    m_sendInput->insertItem(0, text);
    m_sendInput->setCurrentText(text);
    
    while (m_sendInput->count() > 50) {
        m_sendInput->removeItem(m_sendInput->count() - 1);
    }

    QFile historyFile("history.txt");
    if (historyFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&historyFile);
        for (int i = 0; i < m_sendInput->count(); ++i) {
            out << m_sendInput->itemText(i) << "\n";
        }
        historyFile.close();
    }
}

void MainWindow::logEvent(const QString &type, const QByteArray &data, bool streaming) {
    LogEntry entry = { type, data, streaming };
    m_logEntries.append(entry);
    if (m_logEntries.size() > 5000) m_logEntries.removeFirst();
    if (m_rebuilding) {
        m_liveEntriesDuringRebuild.append(entry);
        return;
    }
    renderEntry(m_logEntries.last());
}

void MainWindow::renderEntry(const LogEntry &entry, bool scroll) {
    const bool streamingRx = entry.streaming && entry.type == "RX";
    QString dataText;
    if (entry.type == "INFO") {
        dataText = QString::fromUtf8(entry.data);
    } else if (m_hexCheck->isChecked()) {
        dataText = entry.data.toHex(' ').toUpper();
        if (streamingRx && m_terminalLineOpen && !dataText.isEmpty()) dataText.prepend(' ');
    } else {
        QString currentDecoder = m_decoderCombo->currentText();
        if (currentDecoder != "None") {
            if (streamingRx) {
                dataText = PluginManager::instance().decodeStream(currentDecoder, entry.data);
            } else {
                QHash<QByteArray, QString> &cache = m_decoderCache[currentDecoder];
                auto cached = cache.constFind(entry.data);
                if (cached != cache.cend()) {
                    dataText = cached.value();
                } else {
                    dataText = PluginManager::instance().decode(currentDecoder, entry.data);
                    if (cache.size() >= 5000) cache.clear();
                    cache.insert(entry.data, dataText);
                }
            }
        } else {
            dataText = QString::fromUtf8(entry.data).replace("\r\n", "\n").replace('\r', '\n');
        }
    }
    if (streamingRx && dataText.isEmpty()) return;
    if (!streamingRx && !dataText.endsWith('\n')) dataText.append('\n');
    const bool endsWithNewline = dataText.endsWith('\n');

    const QColor textColor = entry.type == "TX"
        ? QColor(Qt::darkGreen)
        : m_terminalDisplay->palette().color(QPalette::Text);
    QTextCharFormat terminalFormat;
    terminalFormat.setForeground(QBrush(textColor));

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(13, 4);
    blockFormat.setTopMargin(0);
    blockFormat.setBottomMargin(0);

    QTextCursor termCursor(m_terminalDisplay->document());
    termCursor.movePosition(QTextCursor::End);

    // Smart auto-scroll check
    QScrollBar *vBar = m_terminalDisplay->verticalScrollBar();
    bool atBottom = vBar->value() >= (vBar->maximum() - 10);

    QStringList lines = dataText.split('\n');
    if (endsWithNewline) lines.removeLast();
    if (lines.isEmpty()) lines.append("");

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        const bool continuesLine = streamingRx && i == 0 && m_terminalLineOpen;

        // 1. Terminal Panel Insertion
        if (!continuesLine) {
            if (!termCursor.atBlockStart()) termCursor.insertBlock(blockFormat);
            else termCursor.setBlockFormat(blockFormat);
        }

        termCursor.insertText(line, terminalFormat);

        const bool lineBreakAfter = i + 1 < lines.size() || endsWithNewline;
        if (lineBreakAfter) {
            termCursor.insertBlock(blockFormat);
        }
        m_terminalLineOpen = !lineBreakAfter;
    }

    if (scroll && atBottom) {
        m_terminalDisplay->ensureCursorVisible();
        vBar->setValue(vBar->maximum());
    }
}

void MainWindow::rebuildTerminal() {
    const QString currentDecoder = m_decoderCombo->currentText();
    if (currentDecoder != "None") PluginManager::instance().resetStream(currentDecoder);

    m_rebuildTimer->stop();
    m_terminalLineOpen = false;
    m_rebuilding = true;
    m_rebuildIndex = 0;
    m_rebuildEntries = m_logEntries;
    m_liveEntriesDuringRebuild.clear();
    m_terminalDisplay->setUpdatesEnabled(false);
    m_terminalDisplay->clear();
    m_rebuildTimer->start(1);
}

void MainWindow::rebuildTerminalChunk() {
    constexpr int entriesPerChunk = 20;
    int entriesRendered = 0;

    while (m_rebuildIndex < m_rebuildEntries.size()
           && entriesRendered < entriesPerChunk) {
        renderEntry(m_rebuildEntries[m_rebuildIndex], false);
        ++m_rebuildIndex;
        ++entriesRendered;
    }

    if (m_rebuildIndex < m_rebuildEntries.size()) {
        m_rebuildTimer->start(8);
        return;
    }

    m_rebuilding = false;
    m_terminalDisplay->setUpdatesEnabled(true);
    for (const auto &entry : m_liveEntriesDuringRebuild) {
        renderEntry(entry, false);
    }
    m_liveEntriesDuringRebuild.clear();
    m_terminalDisplay->verticalScrollBar()->setValue(m_terminalDisplay->verticalScrollBar()->maximum());
}

void MainWindow::clearTerminal() {
    m_logEntries.clear();
    m_decoderCache.clear();
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
