#include "network/ChannelLogoLoader.h"

#include <QNetworkReply>
#include <QPixmapCache>
#include <QPainter>

#include "core/Logger.h"
#include "network/NetworkManager.h"

namespace {

constexpr int kMaxLogoBytes = 5 * 1024 * 1024;
constexpr int kLogoPixelSize = 32;

} // namespace

ChannelLogoLoader::ChannelLogoLoader(NetworkManager *network, QObject *parent)
    : QObject(parent), m_network(network)
{
}

QString ChannelLogoLoader::cacheKey(const QString &url)
{
    return QStringLiteral("copper/logo:") + url;
}

QPixmap ChannelLogoLoader::cached(const QString &url)
{
    QPixmap pixmap;
    if (QPixmapCache::find(cacheKey(url), &pixmap))
        return pixmap;
    return QPixmap();
}

QPixmap ChannelLogoLoader::placeholder()
{
    static QPixmap generated;
    if (generated.isNull()) {
        generated = QPixmap(kLogoPixelSize, kLogoPixelSize);
        generated.fill(Qt::transparent);
        QPainter p(&generated);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x4a, 0x4e, 0x58));
        p.drawRoundedRect(1, 1, kLogoPixelSize - 2, kLogoPixelSize - 2, 6, 6);
        p.setBrush(QColor(0x9a, 0xa0, 0xab));
        p.drawPolygon(QPolygon(QVector<QPoint>{QPoint(12, 8),
                                                QPoint(24, 16),
                                                QPoint(12, 24)}));
        p.end();
    }
    return generated;
}

void ChannelLogoLoader::requestLogo(const QString &url) const
{
    if (url.isEmpty() || m_inFlight.contains(url))
        return;
    if (QPixmapCache::find(cacheKey(url), nullptr))
        return;

    const QUrl parsed(url);
    if (!(parsed.scheme() == QStringLiteral("http") ||
          parsed.scheme() == QStringLiteral("https")))
        return;

    if (!m_network)
        return;

    QNetworkReply *reply = m_network->accessManager()->get(
        m_network->createRequest(parsed));
    m_inFlight.insert(url);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url]() { handleReply(reply, url); });
}

void ChannelLogoLoader::handleReply(QNetworkReply *reply, const QString &url) const
{
    reply->deleteLater();
    m_inFlight.remove(url);

    if (reply->error() != QNetworkReply::NoError) {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "[logo]" << (status ? status : -1)
                 << "failed for" << url.left(120);
        return;
    }

    const QByteArray data = reply->readAll();
    if (data.isEmpty() || data.size() > kMaxLogoBytes)
        return;

    QPixmap loaded;
    if (!loaded.loadFromData(data))
        return;

    loaded = loaded.scaled(kLogoPixelSize, kLogoPixelSize,
                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (loaded.isNull())
        return;

    QPixmapCache::insert(cacheKey(url), loaded);
    auto *self = const_cast<ChannelLogoLoader *>(this);
    emit self->logoReady(url);
}