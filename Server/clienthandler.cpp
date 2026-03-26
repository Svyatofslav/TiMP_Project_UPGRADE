#include "clienthandler.h"
#include "functions_to_server.h"
#include "DataBase.h"
#include <QDebug>
#include <QtEndian>
#include <cstring>
#include "SessionManager.h"

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QThread(parent), m_socketDescriptor(socketDescriptor)
    , m_clientMode(ClientMode::TEXT)
    , m_modeDetected(false)
{
}

void ClientHandler::run()
{
    QTcpSocket socket;
    if (!socket.setSocketDescriptor(m_socketDescriptor)) {
        qDebug() << "Ошибка дескриптора:" << socket.errorString();
        return;
    }

    qDebug() << "[ClientHandler" << m_socketDescriptor << "] Клиент подключён:" << socket.peerAddress().toString();

    SessionManager::getInstance()->registerKickFlag(
        QString::number(m_socketDescriptor), &m_kicked);

    QByteArray buffer;
    const int MAX_BUFFER_SIZE = 10 * 1024 * 1024;  // 10 МБ — защита от DoS

    while (socket.state() == QAbstractSocket::ConnectedState)
    {
        if (socket.waitForReadyRead(3000))
        {
            QByteArray chunk = socket.readAll();

            // ↓ DoS-защита: не даём буферу разрастись
            if (buffer.size() + chunk.size() > MAX_BUFFER_SIZE) {
                qDebug() << "[ClientHandler" << m_socketDescriptor << "] Буфер переполнен ("
                         << (buffer.size() + chunk.size()) << " байт), отключаем";
                socket.disconnectFromHost();
                break;
            }

            buffer.append(chunk);


            if (!m_modeDetected && buffer.size() >= 4) {
                m_clientMode   = detectClientMode(buffer.left(4));
                m_modeDetected = true;
                qDebug() << "[ClientHandler" << m_socketDescriptor << "]"
                         << socket.peerAddress().toString()
                         << "Режим клиента:"
                         << (m_clientMode == ClientMode::BINARY ? "BINARY" : "TEXT");
            }

            if (m_kicked.load()) {
                qDebug() << "[ClientHandler" << m_socketDescriptor << "] m_kicked обнаружен, отправляю kicked";
                sendMessage(socket, "kicked: вы вошли с другого устройства");
                socket.disconnectFromHost();
                break;
            }

            if (m_modeDetected) {
                if (m_clientMode == ClientMode::BINARY)
                    processBinaryMode(buffer, socket);
                else
                    processTextMode(buffer, socket);
            }
        }
        else {
            if (m_kicked.load()) {
                qDebug() << "[ClientHandler" << m_socketDescriptor << "] m_kicked обнаружен, отправляю kicked";
                sendMessage(socket, "kicked: вы вошли с другого устройства");
                socket.disconnectFromHost();
                break;
            }
            if (socket.state() != QAbstractSocket::ConnectedState)
                break;
        }
    }

    if (socket.state() == QAbstractSocket::ConnectedState) {
        socket.disconnectFromHost();
        socket.waitForDisconnected(3000);
    }

    QString sid = QString::number(m_socketDescriptor);
    QString login = SessionManager::getInstance()->getLogin(sid);
    if (!login.isEmpty()) {
        SessionManager::getInstance()->logout(sid);
        DataBase::getInstance()->clearTaskState(login);
        qDebug() << "[ClientHandler" << m_socketDescriptor << "] Авто-logout для:" << login;
    }

    SessionManager::getInstance()->unregisterKickFlag(
        QString::number(m_socketDescriptor));

    DataBase::getInstance()->closeThreadConnection();
    qDebug() << "[ClientHandler" << m_socketDescriptor << "] Поток клиента завершён";
}

// ↓ Улучшенное зондирование: сначала проверяем длину (BINARY > 10000 → точно BINARY)
ClientHandler::ClientMode ClientHandler::detectClientMode(const QByteArray& first4)
{
    // Сначала проверяем на известные TEXT-команды
    QString preview = QString::fromUtf8(first4).toLower();
    if (preview.startsWith("auth") || preview.startsWith("task") ||
        preview.startsWith("reg")  || preview.startsWith("stat") ||
        preview.startsWith("chec") || preview.startsWith("logo"))
        return TEXT;

    // Иначе — BINARY (первые 4 байта = длина сообщения)
    return BINARY;
}

void ClientHandler::processBinaryMode(QByteArray& buffer, QTcpSocket& socket) {

    if (!m_readySent) {
        sendMessage(socket, "server_ready||1.0");
        m_readySent = true;
    }

    const quint32 MAX_MSG_LEN = 1024 * 1024; // 1 МБ
    int pos = 0;

    while (buffer.size() - pos >= 4) {
        quint32 rawLen = 0;
        memcpy(&rawLen, buffer.constData() + pos, sizeof(quint32));
        quint32 msgLen = qFromBigEndian(rawLen);

        if (msgLen == 0 || msgLen > MAX_MSG_LEN) {
            qWarning() << "[BINARY" << m_socketDescriptor << "] Неверная длина:" << msgLen << ", отключаю клиента";
            socket.disconnectFromHost();
            return;
        }

        if (buffer.size() - pos < 4 + static_cast<int>(msgLen))
            break;

        QByteArray msgData = buffer.mid(pos + 4, static_cast<qsizetype>(msgLen));
        QString request = QString::fromUtf8(msgData);

        pos += 4 + static_cast<int>(msgLen);
        qDebug()   << "[BINARY" << m_socketDescriptor << "] Запрос:" << request;
        QString response = parsing(request, (int)m_socketDescriptor);
        sendMessage(socket, response);
    }

    buffer.remove(0, pos);
}

void ClientHandler::processTextMode(QByteArray& buffer, QTcpSocket& socket)
{
    const int MAX_LINE_LEN = 64 * 1024; // 64 КБ

    if (buffer.size() > MAX_LINE_LEN) {
        qWarning() << "[TEXT" << m_socketDescriptor << "] Слишком длинная строка без CRLF, отключаю клиента";
        socket.disconnectFromHost();
        return;
    }

    int pos = buffer.indexOf("\r\n");
    while (pos != -1) {
        QString request = QString::fromUtf8(buffer.constData(), pos).trimmed();
        buffer.remove(0, pos + 2);

        if (!request.isEmpty()) {
            qDebug()   << "[TEXT" << m_socketDescriptor << "] Запрос:" << request;
            QString response = parsing(request, (int)m_socketDescriptor);
            sendMessage(socket, response + "\r\n");
        }
        pos = buffer.indexOf("\r\n");
    }
}

void ClientHandler::sendMessage(QTcpSocket& socket, const QString& msg)
{
    if (socket.state() != QAbstractSocket::ConnectedState) return;

    if (m_clientMode == ClientMode::BINARY) {
        QByteArray data = msg.toUtf8();
        quint32 len = data.size();
        QByteArray framed(4 + len, 0);
        qToBigEndian(len, (uchar*)framed.data());
        memcpy(framed.data() + 4, data.constData(), len);
        if (socket.write(framed) == -1)
            qWarning() << "[sendMessage" << m_socketDescriptor << "] Ошибка write:" << socket.errorString();
    } else {
        if (socket.write(msg.toUtf8()) == -1)
            qWarning() << "[sendMessage" << m_socketDescriptor << "] Ошибка write:" << socket.errorString();
    }
    socket.flush();
}

