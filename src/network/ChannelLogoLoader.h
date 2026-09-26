#pragma once

#include <QObject>
#include <QPixmap>
#include <QSet>

class NetworkManager;
class QNetworkReply;

// Asynchronously downloads and caches channel logos. Never blocks the GUI:
// decoded logos are stored in QPixmapCache and the delegate is notified via
// logoReady() so it can repaint. Failures fall back to a shared placeholder.
class ChannelLogoLoader : public QObject
{
    Q_OBJECT
public:
    explicit ChannelLogoLoader(NetworkManager *network, QObject *parent = nullptr);

    // Requests a logo; emits logoReady(url) once the pixmap is cached.
    void requestLogo(const QString &url) const;

    static QPixmap placeholder();
    static QPixmap cached(const QString &url);
    static QString cacheKey(const QString &url);

signals:
    void logoReady(const QString &url);

private:
    void handleReply(QNetworkReply *reply, const QString &url) const;

    NetworkManager *m_network = nullptr;
    mutable QSet<QString> m_inFlight;
};