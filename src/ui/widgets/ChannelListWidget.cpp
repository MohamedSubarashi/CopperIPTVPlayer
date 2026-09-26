#include "ui/widgets/ChannelListWidget.h"

#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "network/ChannelLogoLoader.h"
#include "ui/models/ChannelFilterProxyModel.h"
#include "ui/models/ChannelItemDelegate.h"
#include "ui/models/ChannelListModel.h"

namespace {

class ChannelListView : public QListView
{
    Q_OBJECT
public:
    using QListView::QListView;

signals:
    void returnPressed();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (currentIndex().isValid()) {
                emit returnPressed();
                event->accept();
                return;
            }
        }
        QListView::keyPressEvent(event);
    }
};

} // namespace

ChannelListWidget::ChannelListWidget(ChannelLogoLoader *logoLoader,
                                     QWidget *parent)
    : QWidget(parent), m_logoLoader(logoLoader)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search channels..."));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(tr("Search channels"));

    m_sortButton = new QToolButton(this);
    m_sortButton->setText(QStringLiteral("A-Z"));
    m_sortButton->setCheckable(true);
    m_sortButton->setToolTip(tr("Sort channels alphabetically (A-Z)"));
    m_sortButton->setAccessibleName(tr("Sort alphabetically"));
    m_sortButton->setStatusTip(tr("Toggle alphabetical channel sorting"));
    m_sortButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_sortButton->setFixedSize(40, 26);

    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(6);
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(m_sortButton);
    layout->addLayout(searchRow);

    auto *stack = new QStackedWidget(this);
    m_stack = stack;

    m_view = new ChannelListView(stack);
    m_view->setUniformItemSizes(true);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setMouseTracking(true);

    m_delegate = new ChannelItemDelegate(m_logoLoader, this);
    m_view->setItemDelegate(m_delegate);

    m_emptyPage = new QWidget(stack);
    m_emptyLabel = new QLabel(m_emptyPage);
    m_emptyLabel->setObjectName(QStringLiteral("emptyHint"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    auto *emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->addStretch();
    emptyLayout->addWidget(m_emptyLabel);
    emptyLayout->addStretch();

    stack->addWidget(m_view);
    stack->addWidget(m_emptyPage);
    layout->addWidget(stack, 1);

    auto *channelListView = qobject_cast<ChannelListView *>(m_view);
    connect(channelListView, &ChannelListView::returnPressed, this, [this]() {
        if (const auto index = m_view->currentIndex(); index.isValid()) {
            const Channel c =
                index.data(ChannelListModel::ChannelRole).value<Channel>();
            if (c.isValid())
                emit channelActivated(c);
        }
    });
    connect(m_view, &QListView::clicked, this,
            [this](const QModelIndex &index) {
                const Channel c =
                    index.data(ChannelListModel::ChannelRole).value<Channel>();
                if (c.isValid())
                    emit channelClicked(c);
            });
    connect(m_view, &QListView::doubleClicked, this,
            [this](const QModelIndex &index) {
                const Channel c =
                    index.data(ChannelListModel::ChannelRole).value<Channel>();
                if (c.isValid())
                    emit channelActivated(c);
            });
    connect(m_search, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                if (m_proxy)
                    m_proxy->setSearchText(text);
                emit searchTextChanged(text);
                updateEmptyState();
            });

    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QListView::customContextMenuRequested, this,
            &ChannelListWidget::showContextMenu);

    connect(m_sortButton, &QToolButton::toggled, this,
            [this](bool checked) {
                setAlphabeticalSort(checked);
                emit sortOrderChanged(checked);
            });

    if (m_logoLoader) {
        connect(m_logoLoader, &ChannelLogoLoader::logoReady, m_view,
                [this](const QString &) {
                    if (m_view)
                        m_view->viewport()->update();
                });
    }

    updateEmptyState();
}

void ChannelListWidget::setModels(ChannelListModel *model,
                                  ChannelFilterProxyModel *proxy)
{
    m_model = model;
    m_proxy = proxy;
    if (m_proxy) {
        m_view->setModel(m_proxy);
        // The selection model only exists after setModel(); wiring it during
        // construction would silently drop the connection (null receiver).
        connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
                this,
                [this](const QModelIndex &current, const QModelIndex &) {
                    const Channel c =
                        current.data(ChannelListModel::ChannelRole).value<Channel>();
                    if (c.isValid())
                        emit channelSelected(c);
                });
        connect(m_proxy, &QAbstractItemModel::rowsInserted, this,
                &ChannelListWidget::updateEmptyState);
        connect(m_proxy, &QAbstractItemModel::rowsRemoved, this,
                &ChannelListWidget::updateEmptyState);
        connect(m_proxy, &QAbstractItemModel::modelReset, this,
                &ChannelListWidget::updateEmptyState);
    }
    updateEmptyState();
}

ChannelFilterProxyModel *ChannelListWidget::proxy() const
{
    return m_proxy;
}

void ChannelListWidget::setCompact(bool compact)
{
    m_delegate->setCompact(compact);
}

void ChannelListWidget::setAlphabeticalSort(bool enabled)
{
    if (m_proxy)
        m_proxy->setAlphabeticalSort(enabled);
    if (m_sortButton)
        m_sortButton->setChecked(enabled);
}

void ChannelListWidget::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

QString ChannelListWidget::searchText() const
{
    return m_search->text();
}

int ChannelListWidget::rowCount() const
{
    return m_proxy ? m_proxy->rowCount() : 0;
}

void ChannelListWidget::updateEmptyState()
{
    if (!m_stack)
        return;
    const int rows = m_proxy ? m_proxy->rowCount() : 0;
    if (rows > 0) {
        m_emptyStackIndex = 0;
    } else {
        const bool filtering = !m_search->text().isEmpty();
        m_emptyLabel->setText(filtering
                                  ? tr("No channels match your search.")
                                  : tr("No channels found."));
        m_emptyStackIndex = 1;
    }
    m_stack->setCurrentIndex(m_emptyStackIndex);
}

void ChannelListWidget::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;

    const Channel channel = index.data(ChannelListModel::ChannelRole).value<Channel>();
    if (!channel.isValid())
        return;

    QMenu menu(this);
    QAction *playAction = menu.addAction(tr("Play"));
    menu.addSeparator();
    QAction *favAction =
        menu.addAction(channel.isFavorite ? tr("Remove from Favorites")
                                          : tr("Add to Favorites"));
    menu.addSeparator();
    QAction *copyUrlAction = menu.addAction(tr("Copy Stream URL"));
    QAction *copyNameAction = menu.addAction(tr("Copy Channel Name"));
    QAction *infoAction = menu.addAction(tr("Open Channel Information"));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete Channel"));

    QAction *chosen = menu.exec(m_view->viewport()->mapToGlobal(pos));
    if (chosen == playAction) {
        emit playRequested(channel);
    } else if (chosen == favAction) {
        emit toggleFavoriteRequested(channel);
    } else if (chosen == copyUrlAction) {
        emit copyUrlRequested(channel);
    } else if (chosen == copyNameAction) {
        emit copyNameRequested(channel);
    } else if (chosen == infoAction) {
        emit infoRequested(channel);
    } else if (chosen == deleteAction) {
        emit deleteRequested(channel);
    }
}

#include "ChannelListWidget.moc"