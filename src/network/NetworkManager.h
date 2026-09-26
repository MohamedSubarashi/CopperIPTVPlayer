#pragma once

#include <QObject>
#include <QNetworkRequest>

class QNetworkAccessManager;

// Thin wrapper around QNetworkAccessManager that centralizes request policy:
// redirect handling, User-Agent, and timeout helpers. All network access in
// the app goes through here.
class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);

    QNetworkAccessManager *accessManager() const;

    // Creates a GET-ready request with app policy applied.
    QNetworkRequest createRequest(const QUrl &url) const;

    QString userAgent() const;
    void setUserAgent(const QString &ua);

private:
    QNetworkAccessManager *m_nam = nullptr;
    QString m_userAgent;
};