#pragma once

#include <QUrl>
#include <QWidget>

#include "models/Channel.h"

class ChannelFilterProxyModel;
class ChannelItemDelegate;
class ChannelListModel;
class ChannelLogoLoader;
class QLabel;
class QLineEdit;
class QListView;
class QStackedWidget;
class QToolButton;

// Search box + channel list with empty-state handling and a context menu.
class ChannelListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChannelListWidget(ChannelLogoLoader *logoLoader,
                               QWidget *parent = nullptr);

    void setModels(ChannelListModel *model, ChannelFilterProxyModel *proxy);
    QListView *listView() const { return m_view; }
    ChannelFilterProxyModel *proxy() const;
    void setCompact(bool compact);
    void setAlphabeticalSort(bool enabled);
    void focusSearch();
    QString searchText() const;
    int rowCount() const;

signals:
    void channelSelected(const Channel &channel);
    void channelClicked(const Channel &channel);
    void channelActivated(const Channel &channel);
    void searchTextChanged(const QString &text);
    void playRequested(const Channel &channel);
    void toggleFavoriteRequested(const Channel &channel);
    void copyUrlRequested(const Channel &channel);
    void copyNameRequested(const Channel &channel);
    void infoRequested(const Channel &channel);
    void deleteRequested(const Channel &channel);
    void sortOrderChanged(bool alphabetical);

private slots:
    void updateEmptyState();

private:
    void showContextMenu(const QPoint &pos);

    ChannelLogoLoader *m_logoLoader = nullptr;
    ChannelListModel *m_model = nullptr;
    ChannelFilterProxyModel *m_proxy = nullptr;
    ChannelItemDelegate *m_delegate = nullptr;
    QLineEdit *m_search = nullptr;
    QToolButton *m_sortButton = nullptr;
    QListView *m_view = nullptr;
    QStackedWidget *m_stack = nullptr;
    int m_emptyStackIndex = 0;
    QWidget *m_emptyPage = nullptr;
    QLabel *m_emptyLabel = nullptr;
};