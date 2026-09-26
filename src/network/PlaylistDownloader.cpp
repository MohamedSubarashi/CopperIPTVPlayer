#include "network/PlaylistDownloader.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QTimer>

#include "core/Logger.h"
#include "network/NetworkManager.h"

PlaylistDownloader::PlaylistDownloader(NetworkManager *network, QObject *parent)
    : QObject(parent), m_network(network)
{
}

void PlaylistDownloader::download(const QUrl &url, int timeoutMs,
                                  bool ignoreTlsErrors, int token)
{
    if (!m_network) {
        Result result;
        result.url = url.toString();
        result.error = QStringLiteral("Network manager is unavailable.");
        emit finished(token, result);
        return;
    }

    if (!url.isValid() || !QNetworkRequest(url).url().isValid() ||
        !(url.scheme() == QStringLiteral("http") ||
          url.scheme() == QStringLiteral("https"))) {
        Result result;
        result.url = url.toString();
        result.error = QStringLiteral("The playlist URL is invalid.");
        result.detail = QStringLiteral("Unsupported scheme '%1'.").arg(url.scheme());
        emit finished(token, result);
        return;
    }

    const QNetworkRequest request = m_network->createRequest(url);
    QNetworkReply *reply = m_network->accessManager()->get(request);
    m_tokens.insert(reply, token);
    m_ignoreTls.insert(reply, ignoreTlsErrors);

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, reply]() {
        m_timedOut.insert(reply);
        reply->abort();
    });
    timer->start(timeoutMs);
    m_timers.insert(reply, timer);

    connect(reply, &QNetworkReply::sslErrors, this,
            [this, reply](const QList<QSslError> &errors) {
                if (m_ignoreTls.value(reply, false)) {
                    reply->ignoreSslErrors();
                } else {
                    qWarning().noquote()
                        << "[network] TLS errors for"
                        << reply->url().toDisplayString().left(120)
                        << (errors.isEmpty() ? QString()
                                             : errors.first().errorString());
                    m_tlsFailed.insert(reply);
                    reply->abort();
                }
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleFinished(reply);
    });
}

void PlaylistDownloader::handleFinished(QNetworkReply *reply)
{
    Result result;
    result.url = reply->url().toString();

    const int token = m_tokens.take(reply);
    m_ignoreTls.remove(reply);

    if (QTimer *timer = m_timers.take(reply)) {
        timer->stop();
        timer->deleteLater();
    }

    const bool wasTimedOut = m_timedOut.remove(reply);
    const bool wasTlsFailure = m_tlsFailed.remove(reply);

    if (wasTimedOut) {
        result.error = QStringLiteral("The request timed out.");
        result.detail = QStringLiteral("Timeout exceeded.");
        qWarning().noquote() << "[network] timeout" << result.url.left(160);
    } else if (wasTlsFailure) {
        result.error = QStringLiteral("The server's certificate could not be validated.");
        result.detail = QStringLiteral("TLS validation failed. Enable "
                                       "'Ignore TLS errors' in settings to bypass.");
        qWarning().noquote() << "[network] TLS failure" << result.url.left(160);
    } else if (reply->error() != QNetworkReply::NoError) {
        result.error = QStringLiteral("Network error: %1.")
                           .arg(reply->errorString().trimmed());
        result.detail = reply->errorString();
        qWarning().noquote() << "[network] error" << result.url.left(160)
                             << reply->errorString();
    } else {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            result.data = reply->readAll();
            result.ok = true;
        } else {
            result.error = QStringLiteral("Server returned HTTP %1.").arg(status);
            result.detail =
                QStringLiteral("HTTP %1 for %2").arg(status).arg(result.url);
            qWarning().noquote() << "[network] http" << status << result.url.left(160);
        }
    }

    reply->deleteLater();
    emit finished(token, result);
}