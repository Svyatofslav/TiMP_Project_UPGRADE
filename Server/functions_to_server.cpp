#include "functions_to_server.h"
#include "DataBase.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <cmath>
#include "SessionManager.h"


// SHA-512
QString handleSHA512(const QString& payload)
{
    QByteArray hash = QCryptographicHash::hash(
        payload.toUtf8(),
        QCryptographicHash::Sha512);
    return hash.toHex();
}

// Парсер команд
QString parsing(QString str, int descriptor)
{
    str = str.trimmed();
    QStringList parts = str.split("||");
    if (parts.size() > 20) return "pars_Error: слишком много параметров";

    QString func = parts[0];
    parts.removeFirst();

    qDebug() << "[Server] Запрос:" << func << "| params:" << parts;

    if      (func == "task1")      return task1(parts, descriptor);
    else if (func == "task2")      return task2(parts, descriptor);
    else if (func == "task3")      return task3(parts, descriptor);
    else if (func == "task4")      return task4(parts, descriptor);
    else if (func == "check_task") return check_task(parts, descriptor);
    else if (func == "auth")       return author(parts, descriptor);
    else if (func == "reg")        return reg(parts, descriptor);
    else if (func == "stat")       return stat(parts, descriptor);
    else if (func == "logout")     return logout(parts, descriptor);

    return "pars_Error: неизвестная команда";
}

// Вспомогательные для task1
static double evalFunc_task1(int funcId, double x)
{
    switch (funcId)
    {
    case 0: return x * x;
    case 1: return x * x * x;
    case 2: return 2.0 * x + 1.0;
    case 3: return std::sin(x);
    case 4: return std::cos(x);
    default: return x * x;
    }
}

QString funcName_task1(int funcId)
{
    switch (funcId)
    {
    case 0: return "f(x) = x^2";
    case 1: return "f(x) = x^3";
    case 2: return "f(x) = 2x + 1";
    case 3: return "f(x) = sin(x)";
    case 4: return "f(x) = cos(x)";
    default: return "f(x) = x^2";
    }
}

// ↓ Улучшенная генерация: x ∈ [-1.57, 1.57] для sin/cos
QString get_random_task1()
{
    int funcId = QRandomGenerator::global()->bounded(0, 5);

    // Сотые доли: -157..157 → -1.57..1.57 (радианы!)
    int a_cents = QRandomGenerator::global()->bounded(-157, 158);
    double a = a_cents / 100.0;

    // Шаг 1.00..4.00 → b = a + [1..4]
    int step_cents = QRandomGenerator::global()->bounded(100, 401);
    double b = a + step_cents / 100.0;

    int n = QRandomGenerator::global()->bounded(2, 9);

    return QString("%1||%2||%3||%4")
        .arg(funcId)
        .arg(a_cents)
        .arg((int)(b * 100))
        .arg(n);
}

double solver_task1(const QString& params)
{
    QStringList p = params.split("||");
    if (p.size() < 4) return 0.0;

    int    funcId = p[0].toInt();
    double a      = p[1].toDouble() / 100.0;  // ↓ сотые → double
    double b      = p[2].toDouble() / 100.0;
    int    n      = p[3].toInt();

    if (n <= 0) return 0.0;

    double h      = (b - a) / n;
    double result = 0.0;

    for (int i = 1; i <= n; ++i)
    {
        double x = a + h * (i - 0.5);
        result  += evalFunc_task1(funcId, x);
    }

    return result * h;
}

QString task1(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "task_Error: не авторизован";

    QString taskParams = get_random_task1();
    QStringList p = taskParams.split("||");

    DataBase::getInstance()->updateCurrTask(login, 1);
    DataBase::getInstance()->updateParams(login, taskParams);

    qDebug() << "[Server] task1 для" << login << "| params:" << taskParams;

    return QString(
               "task1_OK_START\r\n"
               "=== ЗАДАНИЕ 1: Найти значение интеграла методом средних прямоугольников ===\r\n"
               "Функция: %1\r\n"
               "Отрезок: [%2, %3]\r\n"
               "Количество отрезков n = %4\r\n"
               "Формула: I ≈ h * Σ f(a + h*(i - 0.5)), h = (b-a)/n\r\n"
               "Введите ответ (округлите до 2 знаков): check_task||ваш_ответ\r\n"
               "task1_OK_END/\r\n"
               )
        .arg(funcName_task1(p[0].toInt()))
        .arg(p[1].toDouble()/100.0, 0, 'f', 2)
        .arg(p[2].toDouble()/100.0, 0, 'f', 2)
        .arg(p[3]);
}

QString task2(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "task_Error: не авторизован";
    return "task2: задание в разработке";
}

QString task3(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "task_Error: не авторизован";
    return "task3: задание в разработке";
}

QString task4(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "task_Error: не авторизован";
    return "task4: задание в разработке";
}

QString check_task(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "check_Error: не авторизован";

    if (params.isEmpty() || params[0].trimmed().isEmpty())
        return "check_Error: нужен ответ (check_task||ваш_ответ)";

    QStringList taskData = DataBase::getInstance()->getCurrTaskAndParams(login);

    if (taskData.size() < 2 || taskData[0].isEmpty() || taskData[1].isEmpty())
        return "check_Error: нет активного задания. Сначала запросите задание (task1)";

    int     currTask   = taskData[0].toInt();
    QString taskParams = taskData[1];

    bool ok;
    double userAnswer = params[0].trimmed().replace(",", ".").toDouble(&ok);
    if (!ok) return "check_Error: некорректный ответ. Введите число (например: 0.33)";

    double correctAnswer = 0.0;
    if (currTask == 1)
        correctAnswer = solver_task1(taskParams);
    else
    {
        DataBase::getInstance()->clearTaskState(login);
        return "check_Error: неизвестный тип задания";
    }

    bool isCorrect = std::abs(userAnswer - correctAnswer) < 0.01;

    qDebug() << "[Task] Пользователь:" << login
             << "| задание:" << currTask
             << "| ответ:" << userAnswer
             << "| верно:" << isCorrect;


    DataBase::getInstance()->updateTaskScore(login, currTask, isCorrect ? 1 : -1);
    DataBase::getInstance()->clearTaskState(login);

    if (isCorrect)
        return QString("Ответ верный! Правильный ответ: %1")
            .arg(correctAnswer, 0, 'f', 4);
    else
        return QString("Ответ неверный. Ваш ответ: %1 | Правильный ответ: %2")
            .arg(userAnswer, 0, 'f', 4)
            .arg(correctAnswer, 0, 'f', 4);
}

QString reg(QStringList params, int descriptor)
{
    QString alreadyLogged = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (!alreadyLogged.isEmpty())
        return "reg_Error: нельзя зарегистрироваться, вы уже авторизованы";

    if (params.size() < 4)
        return "reg_Error: нужны login, email, password1, password2";

    QString login     = params[0];
    QString email     = params[1];
    QString password1 = params[2];
    QString password2 = params[3];

    if (login.isEmpty() || password1.isEmpty() || email.isEmpty())
        return "reg_Error: поля не могут быть пустыми";

    if (login.length() > 40)
        return "reg_Error: логин слишком длинный";

    if (!email.contains("@") || !email.contains("."))
        return "reg_Error: некорректный email";

    if (password1 != password2)
        return "reg_Error: пароли не совпадают";

    // ↓ ПРОВЕРКА перед INSERT!
    QString existing = DataBase::getInstance()->checkLoginOrEmail(login, email);
    if (!existing.isEmpty()) {
        if (existing == "login")
            return "reg_Error: логин уже занят";
        else
            return "reg_Error: email уже занят";
    }

    QString hashedPassword = handleSHA512(password1);

    bool ok = DataBase::getInstance()->registerUser(login, hashedPassword, email);

    if (ok)
    {
        // проверка на случай если логин уже онлайн (теоретически не должно быть, но защита)
        if (SessionManager::getInstance()->isOnline(login))
            DataBase::getInstance()->clearTaskState(login);

        SessionManager::getInstance()->login(QString::number(descriptor), login);
        qDebug() << "[Reg] Новый пользователь:" << login << "| email:" << email;
        return "reg_OK";
    }
    return "reg_Error: ошибка регистрации";
}

QString author(QStringList params, int descriptor)
{
    QString alreadyLogged = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (!alreadyLogged.isEmpty())
        return "auth_Error: вы уже авторизованы как " + alreadyLogged + "";

    if (params.size() < 2) return "auth_Error: нужны login и password";

    QString login    = params[0];
    QString password = params[1];

    QString hashedPassword = handleSHA512(password);
    QString result = DataBase::getInstance()->authUser(login, hashedPassword);

    if (!result.isEmpty())
    {
        //сброс старого задания при вытеснении сессии
        if (SessionManager::getInstance()->isOnline(login))
            DataBase::getInstance()->clearTaskState(login);

        SessionManager::getInstance()->login(QString::number(descriptor), login);
        qDebug() << "[Auth] Успешный вход:" << login << "| сокет:" << descriptor;
        return "auth_OK";
    }
    qWarning() << "[Auth] Неудачная попытка входа для логина:" << login;
    return "auth_False";
}

QString stat(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "stat_Error: не авторизован";

    return DataBase::getInstance()->getStatsByLogin(login);
}

QString logout(QStringList params, int descriptor)
{
    QString login = SessionManager::getInstance()->getLogin(QString::number(descriptor));
    if (login.isEmpty()) return "logout_Error: не авторизован";

    SessionManager::getInstance()->logout(QString::number(descriptor));
    DataBase::getInstance()->clearTaskState(login);
    return "logout_OK";
}
