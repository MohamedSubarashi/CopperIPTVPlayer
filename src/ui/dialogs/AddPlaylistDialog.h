#pragma once

#include <QDialog>

#include "models/Playlist.h"

class QLineEdit;
class QPushButton;
class QRadioButton;

// Add a playlist either from a local .m3u/.m3u8 file or from an http(s) URL.
class AddPlaylistDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddPlaylistDialog(QWidget *parent = nullptr);

    Playlist::SourceType sourceType() const;
    QString sourceValue() const;
    QString playlistName() const;

private slots:
    void browseForFile();
    void validateInput();

private:
    QRadioButton *m_localRadio = nullptr;
    QRadioButton *m_remoteRadio = nullptr;
    QLineEdit *m_sourceField = nullptr;
    QPushButton *m_browseButton = nullptr;
    QLineEdit *m_nameField = nullptr;
    QPushButton *m_okButton = nullptr;
};