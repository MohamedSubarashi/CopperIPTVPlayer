#include "ui/dialogs/PlaylistManagerDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "playlist/PlaylistManager.h"
#include "ui/dialogs/AddPlaylistDialog.h"

PlaylistManagerDialog::PlaylistManagerDialog(PlaylistManager *manager,
                                             AppSettings *settings,
                                             QWidget *parent)
    : QDialog(parent), m_manager(manager), m_settings(settings)
{
    setWindowTitle(tr("Playlist Manager"));
    resize(720, 420);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Playlists currently available:"), this);
    layout->addWidget(intro);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        {tr("Name"), tr("Source"), tr("Channels"), tr("Last refresh")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    auto *buttons = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("Add"), this);
    auto *editBtn = new QPushButton(tr("Edit"), this);
    auto *refreshBtn = new QPushButton(tr("Refresh"), this);
    auto *removeBtn = new QPushButton(tr("Remove"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    buttons->addWidget(addBtn);
    buttons->addWidget(editBtn);
    buttons->addWidget(refreshBtn);
    buttons->addWidget(removeBtn);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(addBtn, &QPushButton::clicked, this, &PlaylistManagerDialog::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &PlaylistManagerDialog::onEdit);
    connect(refreshBtn, &QPushButton::clicked, this,
            &PlaylistManagerDialog::onRefresh);
    connect(removeBtn, &QPushButton::clicked, this,
            &PlaylistManagerDialog::onRemove);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    if (m_manager) {
        connect(m_manager, &PlaylistManager::playlistsChanged, this,
                &PlaylistManagerDialog::refreshTable);
        connect(m_manager, &PlaylistManager::playlistLoaded, this,
                [this](int, int) { refreshTable(); });
    }

    refreshTable();
}

void PlaylistManagerDialog::refreshTable()
{
    if (!m_manager)
        return;

    const int selected = selectedPlaylistId();

    const QVector<Playlist> playlists = m_manager->playlists();
    m_table->setRowCount(playlists.size());

    for (int row = 0; row < playlists.size(); ++row) {
        const Playlist &p = playlists.at(row);

        auto *nameItem = new QTableWidgetItem(p.name);
        if (!p.enabled)
            nameItem->setText(tr("%1 (disabled)").arg(p.name));
        nameItem->setData(Qt::UserRole, p.id);

        auto *sourceItem = new QTableWidgetItem(p.source);
        auto *countItem = new QTableWidgetItem(QString::number(p.channelCount));
        countItem->setTextAlignment(Qt::AlignCenter);
        auto *refreshItem = new QTableWidgetItem(
            p.lastRefresh.isValid()
                ? QLocale::system().toString(p.lastRefresh.toLocalTime(),
                                             QLocale::ShortFormat)
                : QStringLiteral("-"));

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, sourceItem);
        m_table->setItem(row, 2, countItem);
        m_table->setItem(row, 3, refreshItem);
    }

    // Restore selection.
    if (selected >= 0) {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            if (m_table->item(row, 0)->data(Qt::UserRole).toInt() == selected) {
                m_table->selectRow(row);
                break;
            }
        }
    }
}

void PlaylistManagerDialog::onAdd()
{
    if (!m_manager)
        return;
    AddPlaylistDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    int id = -1;
    if (dialog.sourceType() == Playlist::SourceType::Local)
        id = m_manager->addLocalPlaylist(dialog.sourceValue(), &error);
    else
        id = m_manager->addRemotePlaylist(dialog.sourceValue(), &error);

    if (id < 0) {
        QMessageBox::warning(this, tr("Add Playlist"),
                             tr("Unable to add the playlist.\n%1")
                                 .arg(error.isEmpty() ? tr("Unknown error")
                                                      : error));
        return;
    }

    const QString desired = dialog.playlistName();
    if (!desired.isEmpty())
        m_manager->renamePlaylist(id, desired);

    m_lastAddedId = id;
    refreshTable();
}

void PlaylistManagerDialog::onEdit()
{
    const int id = selectedPlaylistId();
    if (!m_manager || id < 0)
        return;

    const Playlist current = m_manager->playlistById(id);
    if (current.id < 0)
        return;

    QDialog editDialog(this);
    editDialog.setWindowTitle(tr("Edit Playlist"));
    auto *form = new QVBoxLayout(&editDialog);
    auto *nameEdit = new QLineEdit(current.name, &editDialog);
    auto *enabledBox = new QCheckBox(tr("Enabled"), &editDialog);
    enabledBox->setChecked(current.enabled);
    form->addWidget(new QLabel(tr("Name:"), &editDialog));
    form->addWidget(nameEdit);
    form->addWidget(enabledBox);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &editDialog);
    form->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &editDialog,
            &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &editDialog,
            &QDialog::reject);

    if (editDialog.exec() != QDialog::Accepted)
        return;

    const QString newName = nameEdit->text().trimmed();
    if (!newName.isEmpty())
        m_manager->renamePlaylist(id, newName);
    m_manager->setPlaylistEnabled(id, enabledBox->isChecked());
    refreshTable();
}

void PlaylistManagerDialog::onRefresh()
{
    const int id = selectedPlaylistId();
    if (!m_manager || id < 0)
        return;
    m_manager->refreshPlaylist(id);
}

void PlaylistManagerDialog::onRemove()
{
    const int id = selectedPlaylistId();
    if (!m_manager || id < 0)
        return;

    if (m_settings && m_settings->confirmPlaylistRemoval()) {
        const auto answer = QMessageBox::question(
            this, tr("Remove Playlist"),
            tr("Remove the selected playlist? Its cached channels will be "
               "deleted."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    if (!m_manager->removePlaylist(id))
        return;
    m_lastRemovedId = id;
    refreshTable();
}

int PlaylistManagerDialog::selectedPlaylistId() const
{
    const QModelIndexList selected = m_table->selectionModel()
                                         ? m_table->selectionModel()->selectedRows()
                                         : QModelIndexList();
    if (selected.isEmpty())
        return -1;
    return m_table->item(selected.first().row(), 0)->data(Qt::UserRole).toInt();
}