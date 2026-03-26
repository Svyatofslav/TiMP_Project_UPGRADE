#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>

class Server : public QTcpServer
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    bool startServer(quint16 port);
    static const int MAX_CONNECTIONS = 200;
    std::atomic<int> activeConnections{0};

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

#endif // SERVER_H
