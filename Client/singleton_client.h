#ifndef SINGLETON_CLIENT_H
#define SINGLETON_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QDebug>
#include <QByteArray>
#include <QTimer>

class SingletonClient;

class SingletonClientDestroyer
{
private:
    SingletonClient* p_instance = nullptr;
public:
    ~SingletonClientDestroyer();
    void initialize(SingletonClient* p)
    {
        p_instance = p;
    }
};

class SingletonClient : public QObject
{
    Q_OBJECT

private:
    static SingletonClient*        p_instance;
    static SingletonClientDestroyer destroyer;

    QTcpSocket* mTcpSocket;
    QByteArray m_readBuffer;  // ПЕРСИСТЕНТНЫЙ БУФЕР

protected:
    explicit SingletonClient(QObject* parent = nullptr);
    SingletonClient(const SingletonClient&)            = delete;
    SingletonClient& operator=(const SingletonClient&) = delete;
    ~SingletonClient();

    friend class SingletonClientDestroyer;

public:
    static SingletonClient* getInstance();

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void sendTextCommand(const QString& textCommand);

signals:
    void message_from_server(QString msg);
    void connected();
    void disconnected();
    void errorOccurred(QString errorMsg);

private slots:
    void slotServerRead();
    void slotConnected();
    void slotDisconnected();
    void slotError(QAbstractSocket::SocketError socketError);
};

#endif // SINGLETON_CLIENT_H
