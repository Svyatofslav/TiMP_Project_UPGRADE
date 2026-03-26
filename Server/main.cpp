#include <QCoreApplication>
#include <QDebug>
#include "server.h"
#include "DataBase.h"
#include <signal.h>
#include <QMetaObject>
#include "SessionManager.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    auto cleanup = [&]() {
        qDebug() << "[Main] Завершение — сбрасываем сессии...";
        SessionManager::getInstance()->logoutAll();  // RAM
    };

    QObject::connect(&a, &QCoreApplication::aboutToQuit, cleanup);

    ::signal(SIGINT, [](int) {
        QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    });
#ifdef Q_OS_WIN
    ::signal(SIGBREAK, [](int) {
        QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    });
#endif

    QString dbPath = "timp.db";
    QByteArray envPath = qgetenv("DB_PATH");
    if (!envPath.isEmpty())
        dbPath = QString::fromUtf8(envPath);

    qDebug() << "Используем БД по пути:" << dbPath;

    if (!DataBase::getInstance()->init(dbPath))
    {
        qCritical() << "Не удалось открыть БД, завершение";
        return 1;
    }

    Server server;

    if (!server.startServer(44444)) {
        qCritical() << "Не удалось запустить сервер, завершение";
        return 1;
    }

    return a.exec();
}
