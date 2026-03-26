#include "singleton_client.h"
#include <QtEndian>
#include <cstring>

// Статические члены
SingletonClient*         SingletonClient::p_instance = nullptr;
SingletonClientDestroyer SingletonClient::destroyer;

SingletonClientDestroyer::~SingletonClientDestroyer()
{
    delete p_instance;
}

SingletonClient::~SingletonClient()
{
    if (mTcpSocket && mTcpSocket->state() != QAbstractSocket::UnconnectedState)
    {
        mTcpSocket->disconnectFromHost();
        mTcpSocket->waitForDisconnected(3000);
    }
}

SingletonClient::SingletonClient(QObject* parent) : QObject(parent)
{
    mTcpSocket = new QTcpSocket(this);

    connect(mTcpSocket, &QTcpSocket::readyRead,
            this,        &SingletonClient::slotServerRead);
    connect(mTcpSocket, &QTcpSocket::connected,
            this,        &SingletonClient::slotConnected);
    connect(mTcpSocket, &QTcpSocket::disconnected,
            this,        &SingletonClient::slotDisconnected);
    connect(mTcpSocket, &QAbstractSocket::errorOccurred,
            this,        &SingletonClient::slotError);

}

SingletonClient* SingletonClient::getInstance()
{
    if (!p_instance)
    {
        p_instance = new SingletonClient();
        destroyer.initialize(p_instance);
    }
    return p_instance;
}

void SingletonClient::connectToServer(const QString& host, quint16 port)
{
    if (mTcpSocket->state() == QAbstractSocket::UnconnectedState)
        mTcpSocket->connectToHost(host, port);
}

void SingletonClient::disconnectFromServer()
{
    if (mTcpSocket->state() != QAbstractSocket::UnconnectedState)
        mTcpSocket->disconnectFromHost();
}

void SingletonClient::slotServerRead()
{
    m_readBuffer.append(mTcpSocket->readAll());

    int pos = 0;
    while (m_readBuffer.size() - pos >= 4) {
        quint32 rawLen = 0;
        memcpy(&rawLen, m_readBuffer.constData() + pos, sizeof(quint32));
        quint32 msgLen = qFromBigEndian(rawLen);

        if (msgLen == 0 || msgLen > 1024 * 1024) {
            qWarning() << "[RX] Неверная длина пакета:" << msgLen << ", сбрасываю буфер";
            m_readBuffer.clear();
            disconnectFromServer();
            return;
        }

        if (m_readBuffer.size() - pos < 4 + static_cast<int>(msgLen))
            break;

        QByteArray msgData = m_readBuffer.mid(pos + 4, static_cast<qsizetype>(msgLen));
        QString msg = QString::fromUtf8(msgData);
        qDebug() << "[BINARY RX][len=" << msgLen << "]" << msg;
        emit message_from_server(msg);
        pos += 4 + static_cast<int>(msgLen);
    }
    m_readBuffer.remove(0, pos);
}


void SingletonClient::slotConnected()
{
    qDebug() << "[SingletonClient] Подключён к серверу";
    emit connected();
}

void SingletonClient::slotDisconnected()
{
    m_readBuffer.clear();
    qDebug() << "[SingletonClient] Отключён от сервера";
    emit disconnected();
}

void SingletonClient::slotError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    qWarning() << "[SingletonClient] Ошибка сокета:" << mTcpSocket->errorString();
    emit errorOccurred(mTcpSocket->errorString());
}

void SingletonClient::sendTextCommand(const QString& textCommand)
{
    if (mTcpSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[sendTextCommand] Не подключён!";
        emit errorOccurred("Нет соединения с сервером");
        return;
    }

    QByteArray data = textCommand.toUtf8();
    quint32 len = data.size();

    QByteArray framed(4 + len, 0);
    qToBigEndian(len, reinterpret_cast<uchar*>(framed.data()));
    memcpy(framed.data() + 4, data.constData(), len);

    qDebug() << "[TX BINARY] Отправляю:" << textCommand << "(len=" << len << ")";
    mTcpSocket->write(framed);
    mTcpSocket->flush();
}
