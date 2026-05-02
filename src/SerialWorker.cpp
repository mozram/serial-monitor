#include "SerialWorker.h"

SerialWorker::SerialWorker(QObject *parent) : QObject(parent), m_serialPort(new QSerialPort(this)) {
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialWorker::handleReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &SerialWorker::handleError);
}

SerialWorker::~SerialWorker() {
    disconnectPort();
}

void SerialWorker::connectPort(const QString &portName, int baud, int dataBits, int stopBits, int parity) {
    if (m_serialPort->isOpen()) m_serialPort->close();

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baud);
    m_serialPort->setDataBits(static_cast<QSerialPort::DataBits>(dataBits));
    m_serialPort->setStopBits(static_cast<QSerialPort::StopBits>(stopBits));
    m_serialPort->setParity(static_cast<QSerialPort::Parity>(parity));

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        emit connectionStatusChanged(true, "Connected");
    } else {
        emit errorOccurred(m_serialPort->errorString());
    }
}

void SerialWorker::disconnectPort() {
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        emit connectionStatusChanged(false, "Disconnected");
    }
}

void SerialWorker::writeData(const QByteArray &data) {
    if (m_serialPort->isOpen()) {
        m_serialPort->write(data);
    }
}

void SerialWorker::handleReadyRead() {
    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void SerialWorker::handleError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::ResourceError || error == QSerialPort::PermissionError) {
        emit errorOccurred(m_serialPort->errorString());
        disconnectPort();
    }
}
