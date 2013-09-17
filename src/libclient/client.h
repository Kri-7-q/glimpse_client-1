#ifndef CLIENT_H
#define CLIENT_H

#include "settings.h"
#include "testfactory.h"
#include "testscheduler.h"
#include "types.h"

#include <QObject>
#include <QLocale>
#include <QHostAddress>
#include <QStringList>

class QNetworkAccessManager;

////////////////////////////////////////////////////////////

class CLIENT_API Client : public QObject
{
    Q_OBJECT
    Q_ENUMS(Status)
    Q_PROPERTY(Status status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(TestScheduler* scheduler READ scheduler CONSTANT)
    Q_PROPERTY(Settings* settings READ settings CONSTANT)

    explicit Client(QObject *parent = 0);
    ~Client();

public:
    /// Receives the main client instance
    static Client* instance();

    enum Status
    {
        Unregistered,
        Registered
    };

    /// Get the current status
    void setStatus(Status status);
    Status status() const;

    QNetworkAccessManager* networkAccessManager() const;

    QAbstractSocket* managerSocket() const;

    TestScheduler* scheduler() const;

    Settings* settings() const;

public slots:
    bool init();
    void requestTest();
    // TODO: REMOVE THIS!!!!
    void speedTest();

signals:
    void statusChanged();

private:
    class Private;
    Private* d;
};

#endif // CLIENT_H
