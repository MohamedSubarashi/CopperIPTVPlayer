#pragma once

#include <QDialog>
#include <QVector>

#include "models/Channel.h"

class QListWidget;
class QPushButton;

// Lists channels removed from a playlist so they can be restored. Stays open
// across restores; MainWindow refreshes the list via setChannels().
class RestoreDeletedDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RestoreDeletedDialog(const QVector<Channel> &blockedChannels,
                                  QWidget *parent = nullptr);

    void setChannels(const QVector<Channel> &blockedChannels);

signals:
    // Emitted with the channels the user chose to restore.
    void restoreRequested(const QVector<Channel> &channels);

private:
    void rebuildList();

    QListWidget *m_list = nullptr;
    QPushButton *m_restoreAll = nullptr;
    QVector<Channel> m_blocked;
};