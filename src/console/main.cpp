#include "consoletools.h"
#include "client.h"
#include "log/logger.h"
#include "settings.h"
#include "controller/logincontroller.h"

#include <QTextStream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTimer>
#include <QDebug>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#endif // Q_OS_UNIX

LOGGER(main);

class LoginWatcher : public QObject
{
    Q_OBJECT
public:
    LoginWatcher(LoginController* controller)
    : m_controller(controller)
    {
        connect(controller, SIGNAL(statusChanged()), this, SLOT(onStatusChanged()));
    }

private slots:
    void onStatusChanged() {
        switch(m_controller->status()) {
        case LoginController::Error:
            LOG_INFO("Login/Registration failed, quitting.");
            qApp->quit();
            break;

        case LoginController::Finished:
            deleteLater();
            break;

        default:
            break;
        }
    }

protected:
    LoginController* m_controller;
};

struct LoginData {
    enum Type
    {
        Register,
        Login
    };

    Type type;
    QString userId;
    QString password;
};

int main(int argc, char* argv[])
{
    QCoreApplication::setOrganizationDomain("de.hsaugsburg.informatik");
    QCoreApplication::setOrganizationName("HS-Augsburg");
    QCoreApplication::setApplicationName("mPlaneClient");
    QCoreApplication::setApplicationVersion("0.1");

    QCoreApplication app(argc, argv);

    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription("glimpse commandline version");
    parser.addHelpOption();
    parser.addVersionOption();

#ifdef Q_OS_UNIX
    QCommandLineOption userOption("user", "Run as user", "user");
    parser.addOption(userOption);

    //QCommandLineOption groupOption("group", "Run as group", "group");
    //parser.addOption(groupOption);

    QCommandLineOption daemonOption("daemon", "Run as daemon");
    parser.addOption(daemonOption);

    //QCommandLineOption pidFileOption("pidfile", "Set the pidfile", "path");
    //parser.addOption(pidFileOption);

#endif // Q_OS_UNIX

    QCommandLineOption controllerUrl("controller", "Override the controller host", "hostname:port");
    parser.addOption(controllerUrl);

    QCommandLineOption registerAnonymous("register-anonymous", "Register anonymous on server");
    parser.addOption(registerAnonymous);

    QCommandLineOption registerOption("register", "Register on server", "mail");
    parser.addOption(registerOption);

    QCommandLineOption loginOption("login", "Login on server", "mail");
    parser.addOption(loginOption);

    parser.process(app);

    if (parser.isSet(registerOption) && parser.isSet(registerAnonymous)) {
        out << "'--register' and '--register-anonymous' cannot be set on the same time.\n";
        return 1;
    }

    if ((parser.isSet(registerAnonymous)||parser.isSet(registerOption)) && parser.isSet(loginOption)) {
        out << "'--register(-anonymous)' and '--login' cannot be set on the same time.\n";
        return 1;
    }

    QCommandLineOption* passwordOption = NULL;
    LoginData loginData;

    if (parser.isSet(registerOption)) {
        loginData.type = LoginData::Register;
        passwordOption = &registerOption;
    }
    if (parser.isSet(loginOption)) {
        loginData.type = LoginData::Login;
        passwordOption = &loginOption;
    }

    if (passwordOption) {
        loginData.userId = parser.value(*passwordOption);

        ConsoleTools tools;
        do {
            out << "Password: ";
            out.flush();
            loginData.password = tools.readPassword();
        } while(loginData.password.isEmpty());
    }

#ifdef Q_OS_UNIX
    if (parser.isSet(userOption)) {
        QByteArray user = parser.value(userOption).toLatin1();
        passwd* pwd = getpwnam(user.constData());
        if (pwd) {
            if (-1 == setuid(pwd->pw_uid)) {
                out << "Cannot set uid to " << pwd->pw_uid << ": " << strerror(errno) << "\n";
                return 1;
            } else {
                LOG_DEBUG(QString("User id set to %1 (%2)").arg(pwd->pw_uid).arg(QString::fromLatin1(user)));
            }
        } else {
            out << "No user named " << QString::fromLatin1(user) << " found\n";
            return 1;
        }
    }

    // NOTE: This should be the last option since this creates a process fork!
    if (parser.isSet(daemonOption)) {
        pid_t pid = fork();
        if (pid == -1) {
            out << "fork() failed: " << strerror(errno) << "\n";
        } else if (pid > 0) {
            // Fork successful, we're exiting now
            out << "Daemon started with pid " << pid << "\n";
            return 0;
        } else {
            // Child process
            umask(0);

            pid_t sid = setsid();
            if (sid < 0)
                return 1;

            //chdir("/");

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }
    }
#endif // Q_OS_UNIX

    // If there may be anything left, we flush it before the client starts
    out.flush();

    // Initialize the client instance
    Client* client = Client::instance();

    if (!client->init()) {
        LOG_ERROR("Client initialization failed")
        return 1;
    }

    if (parser.isSet(controllerUrl)) {
        client->settings()->config()->setControllerAddress(parser.value(controllerUrl));
    }

    new LoginWatcher(client->loginController());

    if (passwordOption) {
        switch(loginData.type) {
        case LoginData::Register:
            client->loginController()->registration(loginData.userId, loginData.password);
            break;

        case LoginData::Login:
            client->settings()->setUserId(loginData.userId);
            client->settings()->setPassword(loginData.password);
            client->loginController()->login();
            break;
        }
    } else if (parser.isSet(registerAnonymous)) {
        client->loginController()->anonymousRegistration();
    } else {
        if (!client->autoLogin()) {
            LOG_ERROR("No login data found for autologin");
            return 1;
        }
    }

    int value = app.exec();
    out << "Application shutting down.\n";

    Client::instance()->deleteLater();
    QTimer::singleShot(1, &app, SLOT(quit()));
    app.exec();

    return value;
}

#include "main.moc"
