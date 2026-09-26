#include "network/NetworkManager.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
}

QNetworkAccessManager *NetworkManager::accessManager() const
{
    return m_nam;
}

QNetworkRequest NetworkManager::createRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    request.setRawHeader("Accept", "text/plain,text/html,*/*;q=0.8");
    request.setTransferTimeout(60000);
    return request;
}

QString NetworkManager::userAgent() const
{
    return m_userAgent;
}

void NetworkManager::setUserAgent(const QString &ua)
{
    m_userAgent = ua;
}