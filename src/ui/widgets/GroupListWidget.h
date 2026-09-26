#pragma once

#include <QWidget>

#include "ui/models/GroupListModel.h"

class QListView;

// Channel group/category list with count badges and a star marker on the
// Favorites entry.
class GroupListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GroupListWidget(QWidget *parent = nullptr);

    void setModel(GroupListModel *model);
    void selectRow(int row);
    void selectAllChannels();
    void selectFavorites();
    int currentRow() const;
    GroupListModel *model() const { return m_model; }

signals:
    void groupActivated(const GroupListModel::Item &item);

private:
    GroupListModel *m_model = nullptr;
    QListView *m_view = nullptr;
};