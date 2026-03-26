#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "singleton_client.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    SingletonClient* client = SingletonClient::getInstance();

    bool is_connected = false;
    bool waitingForResponse = false;
    bool kickedReconnect = false;

    // Универсальный таймер для ЛЮБОЙ команды
    QTimer commandTimer;
    commandTimer.setSingleShot(true);
    commandTimer.setInterval(10000);

    // Авто-переподключение
    QTimer reconnectTimer;
    reconnectTimer.setSingleShot(true);
    reconnectTimer.setInterval(3000);

    // Подключение
    QObject::connect(client, &SingletonClient::connected, [&]() {
        qDebug() << "Клиент подключён к серверу";
        is_connected       = true;
        waitingForResponse = false;
        commandTimer.stop();
    });

    // Таймаут команды
    QObject::connect(&commandTimer, &QTimer::timeout, [&]() {
        qWarning() << "Таймаут команды — сервер не отвечает";
        waitingForResponse = false;
        client->disconnectFromServer();
        if (!reconnectTimer.isActive())
            reconnectTimer.start();
    });

    // Отключение
    QObject::connect(client, &SingletonClient::disconnected, [&]() {
        qDebug() << "Клиент отключён от сервера";
        is_connected       = false;
        waitingForResponse = false;
        commandTimer.stop();
        if (!reconnectTimer.isActive())
            reconnectTimer.start();
    });

    // Ошибки
    QObject::connect(client, &SingletonClient::errorOccurred, [&](QString errorMsg) {
        qWarning() << "Ошибка клиента:" << errorMsg;
        is_connected = false;
        if (!reconnectTimer.isActive())
            reconnectTimer.start();
    });

    // Авто-переподключение
    QObject::connect(&reconnectTimer, &QTimer::timeout, [&]() {
        qDebug() << "Пытаюсь переподключиться...";
        client->connectToServer("127.0.0.1", 44444);
    });

    // Ответ от сервера
    QObject::connect(client, &SingletonClient::message_from_server, [&](QString msg) {
        qDebug() << "← Сервер:" << msg;

        if (msg.startsWith("kicked:")) {
            qWarning() << "Вы вытеснены — вход с другого устройства";
            kickedReconnect = true;  // ← пометить что реконнект после кика
            return;
        }

        commandTimer.stop();
        waitingForResponse = false;
    });

    // Вспомогательная функция отправки (GUI будет использовать)
    // client->sendTextCommand("auth||login||pass");
    // commandTimer.start();
    // waitingForResponse = true;

    QObject::connect(client, &SingletonClient::connected, [&]() {
        if (kickedReconnect) {
            kickedReconnect = false;  // сбросить
            return;                   // не отправлять auth автоматически
        }
        QTimer::singleShot(1000, [&]() {
            client->sendTextCommand("auth||test||pass123");
        });
    });


    client->connectToServer("127.0.0.1", 44444);

    return a.exec();
}
