#include "ui/models/ChannelItemDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "models/Channel.h"
#include "network/ChannelLogoLoader.h"
#include "ui/models/ChannelListModel.h"

namespace {

const int kLogoSize = 28;
const int kCompactLogoSize = 20;
const int kRowHeightNormal = 50;
const int kRowHeightCompact = 34;

} // namespace

ChannelItemDelegate::ChannelItemDelegate(const ChannelLogoLoader *logoLoader,
                                         QObject *parent)
    : QStyledItemDelegate(parent), m_logoLoader(logoLoader)
{
}

void ChannelItemDelegate::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    emit sizeHintChanged(QModelIndex());
}

QSize ChannelItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(280, m_compact ? kRowHeightCompact : kRowHeightNormal);
}

void ChannelItemDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const Channel channel = index.data(ChannelListModel::ChannelRole).value<Channel>();

    const QRect rect = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const QPalette &pal = option.palette;

    // Background.
    QColor bg = selected ? pal.color(QPalette::Highlight)
                         : pal.color(QPalette::Base);
    if (!selected && hovered) {
        const QColor hl = pal.color(QPalette::Highlight);
        QColor merged(bg);
        merged = QColor::fromRgbF(merged.redF() + (hl.redF() - merged.redF()) * 0.18,
                                  merged.greenF() + (hl.greenF() - merged.greenF()) * 0.18,
                                  merged.blueF() + (hl.blueF() - merged.blueF()) * 0.18);
        bg = merged;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath bgPath;
    const int radius = 5;
    bgPath.addRoundedRect(QRectF(rect).adjusted(2, 2, -2, -2), radius, radius);
    painter->fillPath(bgPath, bg);

    const QColor textColor =
        selected ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Text);
    const QColor mutedColor =
        selected ? pal.color(QPalette::HighlightedText)
                 : pal.color(QPalette::PlaceholderText);

    const int logoSize = m_compact ? kCompactLogoSize : kLogoSize;
    const int gap = 8;
    const int leftMargin = 8;
    const int rightMargin = 6;

    // Favorite star (always reserve space for a stable layout).
    const bool favorite = channel.isFavorite;
    const int starWidth = 18;
    const int contentRight = rect.right() - rightMargin -
                             (favorite || m_compact ? starWidth : 0);

    // Logo.
    const QRect logoRect(leftMargin, rect.top() + (rect.height() - logoSize) / 2,
                         logoSize, logoSize);
    QPixmap logo;
    if (!channel.logoUrl.isEmpty()) {
        logo = ChannelLogoLoader::cached(channel.logoUrl);
        if (logo.isNull() && m_logoLoader) {
            m_logoLoader->requestLogo(channel.logoUrl);
            logo = ChannelLogoLoader::placeholder();
        }
    }
    if (logo.isNull())
        logo = ChannelLogoLoader::placeholder();

    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawPixmap(logoRect, logo);

    // Favorite star.
    if (favorite) {
        const QRectF starRect(rect.right() - rightMargin - starWidth,
                              rect.top() + (rect.height() - starWidth) / 2.0,
                              starWidth, starWidth);
        QFont starFont = option.font;
        starFont.setPixelSize(starWidth - 4);
        painter->setFont(starFont);
        painter->setPen(QColor(0xe8, 0xc1, 0x4f));
        painter->drawText(starRect, Qt::AlignCenter, QStringLiteral("\u2605"));
    }

    // Text.
    const int textLeft = leftMargin + logoSize + gap;
    const QRect textRect(textLeft, rect.top(), contentRight - textLeft,
                         rect.height());

    if (m_compact) {
        QFont nameFont = option.font;
        nameFont.setBold(true);
        nameFont.setPixelSize(12);
        painter->setFont(nameFont);
        painter->setPen(textColor);
        const QString name = channel.name.isEmpty()
                                 ? ChannelUtils::maskedUrl(channel.streamUrl())
                                 : channel.name;
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(
                              name, Qt::ElideRight, textRect.width()));
    } else {
        QFont nameFont = option.font;
        nameFont.setBold(true);
        nameFont.setPixelSize(13);
        painter->setFont(nameFont);
        const QString name = channel.name.isEmpty()
                                 ? ChannelUtils::maskedUrl(channel.streamUrl())
                                 : channel.name;

        const int lineHeight = painter->fontMetrics().height();
        QRect nameRect = textRect.adjusted(0, 5, 0, 0);
        nameRect.setHeight(lineHeight);
        painter->setPen(textColor);
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(
                              name, Qt::ElideRight, nameRect.width()));

        if (!channel.groupTitle.isEmpty()) {
            QFont groupFont = option.font;
            groupFont.setPixelSize(11);
            painter->setFont(groupFont);
            QRect groupRect = textRect.adjusted(0, nameRect.bottom(), 0, -4);
            groupRect.setTop(nameRect.bottom() - 1);
            painter->setPen(mutedColor);
            painter->drawText(groupRect, Qt::AlignTop | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(
                                  channel.groupTitle, Qt::ElideRight,
                                  groupRect.width()));
        }
    }

    painter->restore();
}