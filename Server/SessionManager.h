#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QObject>
#include <atomic>

class SessionManager : public QObject
{
    Q_OBJECT

public:
    static SessionManager* getInstance();

    void    login(const QString& socketID, const QString& login);
    void    logout(const QString& socketID);
    void    logoutAll();          // при shutdown сервера
    QString getLogin(const QString& socketID) const;
    QString getSocket(const QString& login)   const;
    bool    isOnline(const QString& login)    const;
    void registerKickFlag(const QString& socketID, std::atomic<bool>* flag);
    void unregisterKickFlag(const QString& socketID);

private:
    explicit SessionManager(QObject* parent = nullptr) : QObject(parent) {}
    SessionManager(const SessionManager&)            = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    mutable QReadWriteLock          m_lock;
    QHash<QString, QString> m_socketToLogin;  // socketID → login
    QHash<QString, QString> m_loginToSocket;  // login → socketID
    QHash<QString, std::atomic<bool>*> m_socketToKickFlag;
};

#endif
