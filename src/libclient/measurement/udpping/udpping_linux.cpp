#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/errqueue.h>
#include <linux/icmp.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "udpping.h"

UdpPing::UdpPing(QObject *parent)
    : Measurement(parent),
      currentStatus(Unknown)
{
}

UdpPing::~UdpPing()
{
}

Measurement::Status UdpPing::status() const
{
    return currentStatus;
}

void UdpPing::setStatus(Status status)
{
    if (currentStatus != status)
    {
        currentStatus = status;
        emit statusChanged(status);
    }
}

bool UdpPing::prepare(NetworkManager* networkManager, const MeasurementDefinitionPtr& measurementDefinition)
{
    Q_UNUSED(networkManager);
    definition = measurementDefinition.dynamicCast<UdpPingDefinition>();
    return true;
}

bool UdpPing::start()
{
    PingProbe probe;

    setStatus(UdpPing::Running);

    for (quint32 i = 0; i < definition->count; i++)
    {
        memset(&probe, 0, sizeof(probe));
        probe.sock = initSocket();
        if (probe.sock < 0)
        {
            emit error("socket: " + QString(strerror(errno)));
            continue;
        }
        ping(&probe);
        close(probe.sock);
        m_pingProbes.append(probe);
    }

    setStatus(UdpPing::Finished);
    emit finished();

    return true;
}

bool UdpPing::stop()
{
    return true;
}

ResultPtr UdpPing::result() const
{
    QVariantList res;

    foreach (const PingProbe &probe, m_pingProbes)
    {
        if (probe.sendTime > 0 && probe.recvTime > 0)
        {
            res << probe.recvTime - probe.sendTime;
        }
    }

    return ResultPtr(new Result(QDateTime::currentDateTime(), res, QVariant()));
}

int UdpPing::initSocket()
{
    int n = 0;
    int sock = 0;
    int ttl = definition->ttl ? definition->ttl : 64;
    sockaddr_any src_addr;
    sockaddr_any dst_addr;

    memset(&src_addr, 0, sizeof(src_addr));
    memset(&dst_addr, 0, sizeof(dst_addr));

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        emit error("socket: " + QString(strerror(errno)));
        return -1;
    }

    src_addr.sa.sa_family = AF_INET;
    src_addr.sin.sin_port = htons(definition->sourcePort);
    if (bind(sock, (struct sockaddr *) &src_addr, sizeof(src_addr)) < 0)
    {
        emit error("bind: " + QString(strerror(errno)));
        goto cleanup;
    }

    n = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &n, sizeof(n)) < 0)
    {
        emit error("setsockopt SOL_TIMESTAMP: " + QString(strerror(errno)));
        goto cleanup;
    }

    // set TTL
    if (setsockopt(sock, SOL_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
    {
        emit error("setsockopt IP_TTL: " + QString(strerror(errno)));
        goto cleanup;
    }

    // connect
    getAddress(definition->url, &dst_addr);
    dst_addr.sin.sin_port = htons(definition->destinationPort ? definition->destinationPort : 33434);
    if (::connect(sock, (struct sockaddr *) &dst_addr, sizeof(dst_addr)) < 0)
    {
        emit error("connect: " + QString(strerror(errno)));
        goto cleanup;
    }

    // use RECVRR
    n = 1;
    if (setsockopt(sock, SOL_IP, IP_RECVERR, &n, sizeof(n)) < 0)
    {
        emit error("setsockopt IP_RECVERR: " + QString(strerror(errno)));
        goto cleanup;
    }

    return sock;

cleanup:
    close(sock);
    return -1;
}

bool UdpPing::sendData(PingProbe *probe)
{
    struct timeval tv;

    memset(&tv, 0, sizeof(tv));
    gettimeofday(&tv, NULL);
    probe->sendTime = tv.tv_sec * 1e6 + tv.tv_usec;

    if (send(probe->sock, definition->payload, sizeof(definition->payload), 0) < 0)
    {
        emit error("send: " + QString(strerror(errno)));
        return false;
    }
    return true;
}

void UdpPing::receiveData(PingProbe *probe)
{
    struct msghdr msg;
    sockaddr_any from;
    struct iovec iov;
    char buf[1280];
    char control[1024];
    struct cmsghdr *cm;
    struct sock_extended_err *ee = NULL;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &from;
    msg.msg_namelen = sizeof(from);
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // poll here
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = probe->sock;
    pfd.events = POLLIN | POLLERR;
    if (poll(&pfd, 1, definition->receiveTimeout) < 0)
    {
        emit error("poll: " + QString(strerror(errno)));
        goto cleanup;
    }

    if (!pfd.revents)
    {
        goto cleanup;
    }

    if (recvmsg(probe->sock, &msg, MSG_ERRQUEUE) < 0)
    {
        emit error("recvmsg: " + QString(strerror(errno)));
        goto cleanup;
    }

    for (cm = CMSG_FIRSTHDR(&msg); cm; cm = CMSG_NXTHDR(&msg, cm))
    {
        void *ptr = CMSG_DATA(cm);

        if (cm->cmsg_level == SOL_SOCKET)
        {

            if (cm->cmsg_type == SO_TIMESTAMP)
            {
                struct timeval *tv = (struct timeval *) ptr;
                probe->recvTime = tv->tv_sec * 1e6 + tv->tv_usec;
            }
        }
        else if (cm->cmsg_level == SOL_IP)
        {
            if (cm->cmsg_type == IP_RECVERR)
            {
                ee = (struct sock_extended_err *) ptr;

                if (ee->ee_origin != SO_EE_ORIGIN_ICMP)
                {
                    ee = NULL;
                }

                if (ee->ee_type == ICMP_SOURCE_QUENCH || ee->ee_type == ICMP_REDIRECT)
                {
                    goto cleanup;
                }
            }
        }
    }
    if (ee)
    {
        memcpy(&probe->source, SO_EE_OFFENDER(ee), sizeof(probe->source));
        if (ee->ee_type == ICMP_TIME_EXCEEDED && ee->ee_code == ICMP_EXC_TTL)
        {
            emit ttlExceeded(*probe);
        } else if (ee->ee_type == ICMP_DEST_UNREACH)
        {
            emit destinationUnreachable(*probe);
        }
    }
cleanup:
    return;
}

void UdpPing::ping(PingProbe *probe)
{
    // send
    if (sendData(probe))
    {
        receiveData(probe);
    }
}

bool UdpPing::getAddress(const QString &address, sockaddr_any *addr) const
{
    struct addrinfo hints;
    struct addrinfo *rp = NULL, *result = NULL;

    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_IDN;

    if (getaddrinfo(address.toStdString().c_str(), NULL, &hints, &result))
    {
        return false;
    }

    for (rp = result; rp && rp->ai_family != AF_INET; rp = rp->ai_next);

    if (!rp)
    {
        rp = result;
    }

    memcpy(addr, rp->ai_addr, rp->ai_addrlen);

    freeaddrinfo(result);

    return true;
}

// vim: set sts=4 sw=4 et:
