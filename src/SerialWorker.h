#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QThread>

class SerialWorker : public QObject {
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();

public slots:
    void connectPort(const QString &portName, int baud, int dataBits, int stopBits, int parity);
    void disconnectPort();
    void writeData(const QByteArray &data);

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void connectionStatusChanged(bool connected, const QString &msg);

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serialPort;
};

#endif // SERIALWORKER_H
