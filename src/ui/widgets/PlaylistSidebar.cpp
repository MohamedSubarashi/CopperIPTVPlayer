#include "ui/widgets/PlaylistSidebar.h"

#include <QAction>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "core/Constants.h"
#include "ui/models/GroupListModel.h"
#include "ui/models/PlaylistListModel.h"
#include "ui/widgets/GroupListWidget.h"

namespace {

class PlaylistListView : public QListView
{
    Q_OBJECT
public:
    using QListView::QListView;

signals:
    void deleteRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if ((event->key() == Qt::Key_Delete ||
             event->key() == Qt::Key_Backspace) &&
            currentIndex().isValid()) {
            emit deleteRequested();
            event->accept();
            return;
        }
        QListView::keyPressEvent(event);
    }
};

QLabel *headerLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sidebarHeader"));
    return label;
}

} // namespace

PlaylistSidebar::PlaylistSidebar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(headerLabel(tr("Playlists"), this));

    m_playlistView = new PlaylistListView(this);
    m_playlistView->setUniformItemSizes(true);
    m_playlistView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_playlistView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_playlistView->setMinimumHeight(90);
    layout->addWidget(m_playlistView, 1);

    layout->addSpacing(6);
    layout->addWidget(headerLabel(tr("Groups"), this));

    m_groupWidget = new GroupListWidget(this);
    layout->addWidget(m_groupWidget, 2);

    connect(m_playlistView, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (m_playlistModel && index.isValid()) {
            const int id = m_playlistModel
                               ->data(index, PlaylistListModel::PlaylistRole)
                               .value<Playlist>()
                               .id;
            emit playlistActivated(id);
        }
    });
    connect(m_groupWidget, &GroupListWidget::groupActivated, this,
            [this](const GroupListModel::Item &item) {
                emit groupActivated(item.key);
            });
}

void PlaylistSidebar::setPlaylistModel(PlaylistListModel *model)
{
    m_playlistModel = model;
    m_playlistView->setModel(model);
    // The selection model only exists after setModel(); wiring it in the
    // constructor would silently drop the connection (null receiver).
    if (model) {
        connect(m_playlistView->selectionModel(),
                &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex &current, const QModelIndex &) {
                    if (m_playlistModel && current.isValid()) {
                        const int id = m_playlistModel
                                           ->data(current, PlaylistListModel::PlaylistRole)
                                           .value<Playlist>()
                                           .id;
                        emit playlistSelectionChanged(id);
                    }
                });
    }

    m_playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistView, &QListView::customContextMenuRequested, this,
            &PlaylistSidebar::showPlaylistMenu);
    auto *listView = qobject_cast<PlaylistListView *>(m_playlistView);
    if (listView) {
        connect(listView, &PlaylistListView::deleteRequested, this,
                [this]() {
                    const int id = currentPlaylistId();
                    if (id >= 0)
                        emit removePlaylistRequested(id);
                });
    }
}

void PlaylistSidebar::setGroupModel(GroupListModel *model)
{
    m_groupModel = model;
    m_groupWidget->setModel(model);
}

void PlaylistSidebar::selectPlaylistById(int id)
{
    if (!m_playlistModel)
        return;
    for (int row = 0; row < m_playlistModel->rowCount(); ++row) {
        const QModelIndex index = m_playlistModel->index(row);
        const Playlist p = m_playlistModel->data(index, PlaylistListModel::PlaylistRole)
                               .value<Playlist>();
        if (p.id == id) {
            m_playlistView->setCurrentIndex(index);
            return;
        }
    }
}

void PlaylistSidebar::showPlaylistMenu(const QPoint &pos)
{
    if (!m_playlistModel)
        return;

    const QModelIndex index = m_playlistView->indexAt(pos);
    const int id = index.isValid()
                       ? m_playlistModel
                             ->data(index, PlaylistListModel::PlaylistRole)
                             .value<Playlist>()
                             .id
                       : -1;

    QMenu menu(this);
    QAction *refreshAction = nullptr;
    QAction *removeAction = nullptr;
    if (id >= 0) {
        refreshAction = menu.addAction(tr("Refresh Playlist"));
        removeAction = menu.addAction(tr("Remove Playlist"));
    }
    QAction *addAction = menu.addAction(tr("Add Playlist..."));
    QAction *manageAction = menu.addAction(tr("Manage Playlists..."));

    QAction *chosen = menu.exec(m_playlistView->viewport()->mapToGlobal(pos));
    if (chosen == removeAction) {
        emit removePlaylistRequested(id);
    } else if (chosen == refreshAction) {
        emit refreshPlaylistRequested(id);
    } else if (chosen == addAction) {
        emit addPlaylistRequested();
    } else if (chosen == manageAction) {
        emit managePlaylistsRequested();
    }
}

int PlaylistSidebar::currentPlaylistId() const
{
    const QModelIndex index = m_playlistView->currentIndex();
    if (m_playlistModel && index.isValid()) {
        return m_playlistModel
            ->data(index, PlaylistListModel::PlaylistRole)
            .value<Playlist>()
            .id;
    }
    return -1;
}

void PlaylistSidebar::selectGroup(const QString &key)
{
    if (!m_groupModel)
        return;
    const int row = m_groupModel->findRowByKey(key);
    m_groupWidget->selectRow(row);
}

#include "PlaylistSidebar.moc"