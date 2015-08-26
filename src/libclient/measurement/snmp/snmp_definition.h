#ifndef SNMPDEFINITION_H
#define SNMPDEFINITION_H

#include "measurement/measurement.h"
#include <QHostAddress>


class SnmpDefinition;

typedef QSharedPointer<SnmpDefinition> SnmpDefinitionPtr;
typedef QList<SnmpDefinitionPtr> SnmpDefinitionList;

class SnmpDefinition : public MeasurementDefinition
{
public:
    SnmpDefinition(const QStringList &communityList, const int retriesPerIp, const int snmpVersion, const QString &hostAddresses,
                   const int measurementType, const int sendInterval, const int waitTime);
    SnmpDefinition(const int version, const int requestType, const QString &communityName, const QString &host, const QString &objectIdentifier,
                   const QString &value, const int authentication, const int privacy, const QString &username, const QString &password,
                   const QString &contextOID, const int measurementType);
    ~SnmpDefinition();

    QString m_communityName;
    QStringList m_communityList;
    int m_retriesPerIp;
    int m_snmpVersion;
    QString m_hostAddresses;
    int m_measurementType;
    int m_sendInterval;
    int m_waitTime;
    QString m_objectIdentifier;
    int m_authentication;
    int m_privacy;
    QString m_username;
    QString m_password;
    QString m_contextOID;
    int m_requestType;
    QString m_requestValue;
    // Not needed to in 'fromVariant()' and 'toVariant()'
    QHostAddress m_startIP;
    QHostAddress m_endIP;

    static SnmpDefinitionPtr fromVariant(const QVariant &variant);
    QVariant toVariant() const;
};

#endif // SNMPDEFINITION_H
