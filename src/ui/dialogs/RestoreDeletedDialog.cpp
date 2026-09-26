#include "ui/dialogs/RestoreDeletedDialog.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

RestoreDeletedDialog::RestoreDeletedDialog(const QVector<Channel> &blockedChannels,
                                           QWidget *parent)
    : QDialog(parent), m_blocked(blockedChannels)
{
    setWindowTitle(tr("Restore Removed Channels"));
    setMinimumSize(440, 340);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(
        tr("Channels removed from the current playlist are listed below. "
           "Restoring brings them back, even after the playlist is refreshed."),
        this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("emptyHint"));
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_list, 1);

    auto *restoreSelected = new QPushButton(tr("Restore Selected"), this);
    restoreSelected->setAccessibleName(tr("Restore selected channels"));
    auto *restoreAll = new QPushButton(tr("Restore All"), this);
    restoreAll->setAccessibleName(tr("Restore all removed channels"));
    m_restoreAll = restoreAll;
    auto *closeButton = new QPushButton(tr("Close"), this);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(restoreSelected);
    buttons->addWidget(m_restoreAll);
    buttons->addStretch(1);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(restoreSelected, &QPushButton::clicked, this, [this]() {
        QVector<Channel> chosen;
        const auto selected = m_list->selectedItems();
        for (const QListWidgetItem *item : selected) {
            const int index = item->data(Qt::UserRole).toInt();
            if (index >= 0 && index < m_blocked.size())
                chosen.append(m_blocked.at(index));
        }
        if (!chosen.isEmpty())
            emit restoreRequested(chosen);
    });
    connect(restoreAll, &QPushButton::clicked, this, [this]() {
        if (!m_blocked.isEmpty())
            emit restoreRequested(m_blocked);
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    rebuildList();
}

void RestoreDeletedDialog::setChannels(const QVector<Channel> &blockedChannels)
{
    m_blocked = blockedChannels;
    rebuildList();
}

void RestoreDeletedDialog::rebuildList()
{
    m_list->clear();
    for (int i = 0; i < m_blocked.size(); ++i) {
        const Channel &channel = m_blocked.at(i);
        QString text = channel.name.isEmpty()
                           ? tr("(unnamed)")
                           : channel.name;
        if (!channel.groupTitle.isEmpty())
            text += QStringLiteral(" \u2014 %1").arg(channel.groupTitle);
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, i);
        item->setToolTip(channel.streamUrl());
    }
    m_restoreAll->setEnabled(!m_blocked.isEmpty());
    m_list->setEnabled(!m_blocked.isEmpty());
}