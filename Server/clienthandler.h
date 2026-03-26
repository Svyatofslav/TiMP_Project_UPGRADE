#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QThread>
#include <QTcpSocket>
#include <atomic>

class ClientHandler : public QThread
{
    Q_OBJECT
public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);

protected:
    void run() override;

private:
    qintptr m_socketDescriptor;
    enum ClientMode { BINARY, TEXT };
    ClientMode m_clientMode;
    bool m_modeDetected;
    bool m_readySent = false;
    std::atomic<bool> m_kicked{false};
    ClientMode detectClientMode(const QByteArray& first4);
    void processBinaryMode(QByteArray& buffer, QTcpSocket& socket);
    void processTextMode(QByteArray& buffer, QTcpSocket& socket);
    void sendMessage(QTcpSocket& socket, const QString& msg);
};

#endif // CLIENTHANDLER_H
