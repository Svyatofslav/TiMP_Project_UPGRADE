#include "DataBase.h"

DataBase* DataBase::getInstance()
{
    static DataBase instance;
    return &instance;
}

QString DataBase::threadConnName() const
{
    return QString("conn_%1")
    .arg(reinterpret_cast<qulonglong>(QThread::currentThreadId()));
}

// ─── Ключевой метод: потокобезопасный ────────────────────────────────────────
QSqlDatabase DataBase::dbForThread()
{
    const QString name = threadConnName();

    // Сначала читаем без блокировки — если соединение уже есть, возвращаем
    // QSqlDatabase::database() сам по себе потокобезопасен для чтения
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);

    // Создаём новое соединение — только здесь нужна блокировка
    QMutexLocker locker(&m_connMutex);

    // Double-checked locking: пока ждали мьютекс, другой поток мог создать
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        qCritical() << "[DataBase] Поток" << QThread::currentThreadId()
            << "не смог открыть БД:" << db.lastError().text();
        return QSqlDatabase();  // ← ДОБАВЛЕНО: явный возврат невалидного объекта
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL;");
    pragma.exec("PRAGMA synchronous=NORMAL;");
    pragma.exec("PRAGMA foreign_keys=ON;");
    pragma.exec("PRAGMA busy_timeout=5000;");
    qDebug() << "[DataBase] Новое соединение для потока"
             << QThread::currentThreadId();

    return db;
}

// ─── Вызывать в конце каждого QThread::run() ─────────────────────────────────
void DataBase::closeThreadConnection()
{
    const QString name = threadConnName();
    if (!QSqlDatabase::contains(name))
        return;

    {
        QSqlDatabase db = QSqlDatabase::database(name);
        db.close();
    }  // ← db уничтожен здесь
    QMutexLocker locker(&m_connMutex);
    QSqlDatabase::removeDatabase(name);
    qDebug() << "[DataBase] Соединение закрыто для потока"
             << QThread::currentThreadId();
}
// ─── init() ──────────────────────────────────────────────────────────────────
bool DataBase::init(const QString& dbPath)
{
    QMutexLocker locker(&m_connMutex);
    m_dbPath = dbPath;

    const QString initConn = "init_connection";
    bool success = false;

    {   // ← открываем scope для db
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", initConn);
        db.setDatabaseName(m_dbPath);

        if (!db.open()) {
            qCritical() << "[DataBase] Ошибка открытия БД:" << db.lastError().text();
        } else {
            QSqlQuery pragma(db);
            pragma.exec("PRAGMA journal_mode=WAL;");
            pragma.exec("PRAGMA synchronous=NORMAL;");
            pragma.exec("PRAGMA cache_size=10000;");
            pragma.exec("PRAGMA foreign_keys=ON;");
            pragma.exec("PRAGMA busy_timeout=5000;");

            QSqlQuery query(db);
            bool ok = query.exec(
                "CREATE TABLE IF NOT EXISTS Person ("
                "  login    VARCHAR(40)  NOT NULL PRIMARY KEY,"
                "  password CHAR(128)    NOT NULL,"
                "  email    VARCHAR(100) NOT NULL UNIQUE,"
                "  role     VARCHAR(40)  NOT NULL DEFAULT 'user',"
                "  task1    INTEGER NOT NULL DEFAULT 0,"
                "  task2    INTEGER NOT NULL DEFAULT 0,"
                "  task3    INTEGER NOT NULL DEFAULT 0,"
                "  task4    INTEGER NOT NULL DEFAULT 0,"
                "  currtask INTEGER,"
                "  params   VARCHAR(256)"
                ")");

            if (!ok) {
                qCritical() << "[DataBase] Ошибка создания таблицы:"
                            << query.lastError().text();
            } else {
                QSqlQuery cleanup(db);
                bool cleanOk = cleanup.exec(
                    "UPDATE Person SET currtask = NULL, params = NULL "
                    "WHERE currtask IS NOT NULL OR params IS NOT NULL");
                if (!cleanOk)
                    qWarning() << "[DataBase] Ошибка сброса заданий:"
                               << cleanup.lastError().text();
                else
                    qDebug() << "[DataBase] Сброшено незавершённых заданий:"
                             << cleanup.numRowsAffected();

                success = true;
            }
            db.close();
        }
    }   // ← db уничтожен здесь

    QSqlDatabase::removeDatabase(initConn);

    if (success)
        qDebug() << "[DataBase] init() завершён, dbPath =" << m_dbPath;

    return success;
}

// ─── Остальные методы (сессий нет — только данные) ───────────────────────────

QString DataBase::authUser(const QString& login, const QString& password)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare(
        "SELECT login FROM Person WHERE login = :login AND password = :password LIMIT 1");
    query.bindValue(":login",    login);
    query.bindValue(":password", password);

    if (!query.exec()) {
        qWarning() << "[DataBase] authUser ошибка:" << query.lastError().text();
        return "";
    }
    return query.next() ? "OK" : "";
}

bool DataBase::registerUser(const QString& login, const QString& password,
                            const QString& email)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO Person (login, password, email, role, task1, task2, task3, task4) "
        "VALUES (:login, :password, :email, 'user', 0, 0, 0, 0)");
    query.bindValue(":login",    login);
    query.bindValue(":password", password);
    query.bindValue(":email",    email);

    if (!query.exec()) {
        qWarning() << "[DataBase] registerUser ошибка:" << query.lastError().text();
        return false;
    }
    return true;
}

QString DataBase::checkLoginOrEmail(const QString& login, const QString& email)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare(
        "SELECT 'login' FROM Person WHERE login = :login "
        "UNION ALL SELECT 'email' FROM Person WHERE email = :email LIMIT 1");
    query.bindValue(":login", login);
    query.bindValue(":email", email);

    if (!query.exec() || !query.next()) return "";
    return query.value(0).toString();
}

QString DataBase::getStatsByLogin(const QString& login)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare(
        "SELECT login, task1, task2, task3, task4 FROM Person WHERE login = :login");
    query.bindValue(":login", login);

    if (!query.exec() || !query.next())
        return "stat_Error: пользователь не найден\r\n";

    return QString("stat_OK||%1||%2||%3||%4||%5\r\n")
        .arg(query.value(0).toString())
        .arg(query.value(1).toInt())
        .arg(query.value(2).toInt())
        .arg(query.value(3).toInt())
        .arg(query.value(4).toInt());
}

bool DataBase::updateCurrTask(const QString& login, int taskNum)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare("UPDATE Person SET currtask = :ct WHERE login = :login");
    query.bindValue(":ct",    taskNum);
    query.bindValue(":login", login);
    return query.exec();
}

bool DataBase::updateParams(const QString& login, const QString& params)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare("UPDATE Person SET params = :params WHERE login = :login");
    query.bindValue(":params", params);
    query.bindValue(":login",  login);
    return query.exec();
}

QStringList DataBase::getCurrTaskAndParams(const QString& login)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare("SELECT currtask, params FROM Person WHERE login = :login");
    query.bindValue(":login", login);

    QStringList result;
    if (!query.exec() || !query.next()) return result;
    result << query.value(0).toString() << query.value(1).toString();
    return result;
}

bool DataBase::updateTaskScore(const QString& login, int taskNum, int delta)
{
    if (taskNum < 1 || taskNum > 4) return false;

    QSqlDatabase db = dbForThread();
    QString col = QString("task%1").arg(taskNum);
    QSqlQuery query(db);
    query.prepare(
        QString("UPDATE Person SET %1 = %1 + :delta WHERE login = :login").arg(col));
    query.bindValue(":delta", delta);
    query.bindValue(":login", login);
    return query.exec();
}

bool DataBase::clearTaskState(const QString& login)
{
    QSqlDatabase db = dbForThread();
    QSqlQuery query(db);
    query.prepare(
        "UPDATE Person SET currtask = NULL, params = NULL WHERE login = :login");
    query.bindValue(":login", login);
    return query.exec();
}
