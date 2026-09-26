#include "ui/dialogs/ChannelInfoDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/Constants.h"
#include "models/Channel.h"
#include "network/ChannelLogoLoader.h"

ChannelInfoDialog::ChannelInfoDialog(const Channel &channel, bool isFavorite,
                                     ChannelLogoLoader *logoLoader,
                                     QWidget *parent)
    : QDialog(parent), m_channel(channel), m_isFavorite(isFavorite),
      m_logoLoader(logoLoader)
{
    setWindowTitle(tr("Channel Info"));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);

    auto *header = new QHBoxLayout;
    m_logoLabel = new QLabel(this);
    m_logoLabel->setFixedSize(96, 54);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setText(tr("Logo"));
    m_logoLabel->setStyleSheet(
        QStringLiteral("border: 1px solid palette(mid); border-radius: 4px;"));
    header->addWidget(m_logoLabel, 0, Qt::AlignTop);
    header->addWidget(new QLabel(m_channel.name, this), 1, Qt::AlignVCenter);
    header->addSpacing(8);
    layout->addLayout(header);

    if (m_isFavorite) {
        auto *favBox = new QLabel(tr("\u2605 Favorite"), this);
        favBox->setObjectName(QStringLiteral("favoriteBadge"));
        layout->addWidget(favBox);
    }

    auto *form = new QFormLayout;
    fillRow(form, tr("Group:"), m_channel.groupTitle);
    fillRow(form, tr("TVG ID:"), m_channel.tvgId);
    fillRow(form, tr("TVG name:"), m_channel.tvgName);
    fillRow(form, tr("Extgr:"), m_channel.attributes.value(QStringLiteral("extgr")));
    fillRow(form, tr("Country:"), m_channel.country);
    fillRow(form, tr("Language:"), m_channel.language);
    fillRow(form, tr("Bitrate:"), m_channel.attributes.value(QStringLiteral("bitrate")));
    fillRow(form, tr("Status:"), m_channel.attributes.value(QStringLiteral("status")));
    fillRow(form, tr("URL:"),
            ChannelUtils::maskedUrl(m_channel.streamUrl()));
    layout->addLayout(form);

    if (!m_channel.logoUrl.isEmpty() && m_logoLoader) {
        connect(m_logoLoader, &ChannelLogoLoader::logoReady, this,
                [this](const QString &url) {
                    if (url == m_channel.logoUrl) {
                        const QPixmap pixmap =
                            ChannelLogoLoader::cached(m_channel.logoUrl);
                        if (!pixmap.isNull())
                            m_logoLabel->setPixmap(pixmap);
                    }
                });
        m_logoLoader->requestLogo(m_channel.logoUrl);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::reject);
}

void ChannelInfoDialog::fillRow(QFormLayout *layout, const QString &label,
                                const QString &value)
{
    if (value.isEmpty())
        return;
    auto *valueLabel = new QLabel(value, this);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel->setWordWrap(true);
    layout->addRow(label, valueLabel);
}