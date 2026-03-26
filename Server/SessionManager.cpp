#include "SessionManager.h"
#include <QDebug>

SessionManager* SessionManager::getInstance()
{
    static SessionManager instance;
    return &instance;
}

void SessionManager::login(const QString& socketID, const QString& login)
{
    QWriteLocker lock(&m_lock);
    // Если этот логин уже был с другого сокета — чистим старый
    if (m_loginToSocket.contains(login)) {
        QString oldSocket = m_loginToSocket[login];
        m_socketToLogin.remove(oldSocket);
        if (m_socketToKickFlag.contains(oldSocket))
            m_socketToKickFlag[oldSocket]->store(true);
        qWarning() << "[Session] Пользователь" << login
                   << "вытеснен со старого сокета:" << oldSocket
                   << "новым сокетом:" << socketID;
    }
    m_socketToLogin[socketID] = login;
    m_loginToSocket[login]    = socketID;
}

void SessionManager::registerKickFlag(const QString& socketID, std::atomic<bool>* flag) {
    QWriteLocker lock(&m_lock);
    m_socketToKickFlag[socketID] = flag;
}

void SessionManager::unregisterKickFlag(const QString& socketID) {
    QWriteLocker lock(&m_lock);
    m_socketToKickFlag.remove(socketID);
}

void SessionManager::logout(const QString& socketID)
{
    QWriteLocker lock(&m_lock);
    if (m_socketToLogin.contains(socketID)) {
        QString login = m_socketToLogin[socketID];
        m_loginToSocket.remove(login);
        m_socketToLogin.remove(socketID);
    }
}

void SessionManager::logoutAll()
{
    QWriteLocker lock(&m_lock);
    m_socketToLogin.clear();
    m_loginToSocket.clear();
    m_socketToKickFlag.clear();
}

QString SessionManager::getLogin(const QString& socketID) const
{
    QReadLocker lock(&m_lock);
    return m_socketToLogin.value(socketID, "");
}

QString SessionManager::getSocket(const QString& login) const
{
    QReadLocker lock(&m_lock);
    return m_loginToSocket.value(login, "");
}

bool SessionManager::isOnline(const QString& login) const
{
    QReadLocker lock(&m_lock);
    return m_loginToSocket.contains(login);
}
