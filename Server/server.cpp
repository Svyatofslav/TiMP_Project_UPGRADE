#include "server.h"
#include "clienthandler.h"
#include <QDebug>

Server::Server(QObject *parent) : QTcpServer(parent) {}

bool Server::startServer(quint16 port) {
    setMaxPendingConnections(100);
    if (listen(QHostAddress::Any, port)) {
        qDebug() << "Сервер запущен на порту" << port;
        return true;
    }
    qCritical() << "Ошибка запуска:" << errorString();
    return false;
}


void Server::incomingConnection(qintptr socketDescriptor) {
    if (activeConnections >= MAX_CONNECTIONS) {
        qWarning() << "[Server] Лимит соединений достигнут, отклоняю:" << socketDescriptor;
        QTcpSocket reject;
        reject.setSocketDescriptor(socketDescriptor);
        reject.disconnectFromHost();
        reject.waitForDisconnected(1000);
        return;
    }

    activeConnections++;
    qDebug() << "Новый клиент, дескриптор:" << socketDescriptor
             << "| активных:" << activeConnections.load();

    ClientHandler *handler = new ClientHandler(socketDescriptor, this);
    connect(handler, &ClientHandler::finished,
            handler, &ClientHandler::deleteLater);
    connect(handler, &ClientHandler::finished, [this]() {
        activeConnections--;
        qDebug() << "[Server] Поток завершён, активных:" << activeConnections.load();
    });

    handler->start();
    if (!handler->isRunning()) {
        qWarning() << "[Server] Не удалось запустить поток для дескриптора:" << socketDescriptor;
        activeConnections--;
        delete handler;
        QTcpSocket reject;
        reject.setSocketDescriptor(socketDescriptor);
        reject.disconnectFromHost();
        reject.waitForDisconnected(1000);
        return;
    }
}
