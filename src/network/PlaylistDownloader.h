#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QUrl>

class NetworkManager;
class QNetworkReply;
class QTimer;

// Asynchronous download helper for playlist fetching. Safe timeout handling,
// redirect policy (applied by NetworkManager), TLS policy, and HTTP status
// reporting. Results are delivered on the owning thread via signals.
class PlaylistDownloader : public QObject
{
    Q_OBJECT
public:
    struct Result
    {
        bool ok = false;
        QString url;
        QByteArray data;
        QString error; // user-facing, no secrets
        QString detail; // technical detail for the log
    };

    explicit PlaylistDownloader(NetworkManager *network, QObject *parent = nullptr);

    void download(const QUrl &url, int timeoutMs, bool ignoreTlsErrors, int token);

signals:
    void finished(int token, const PlaylistDownloader::Result &result);

private:
    void handleFinished(QNetworkReply *reply);

    NetworkManager *m_network = nullptr;
    QHash<QNetworkReply *, QTimer *> m_timers;
    QHash<QNetworkReply *, int> m_tokens;
    QHash<QNetworkReply *, bool> m_ignoreTls;
    QSet<QNetworkReply *> m_timedOut;
    QSet<QNetworkReply *> m_tlsFailed;
};