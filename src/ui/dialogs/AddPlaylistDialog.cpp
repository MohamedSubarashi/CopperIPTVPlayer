#include "ui/dialogs/AddPlaylistDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QUrl>
#include <QVBoxLayout>

AddPlaylistDialog::AddPlaylistDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Playlist"));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);

    auto *sourceGroup = new QGroupBox(tr("Playlist source"), this);
    auto *sourceLayout = new QGridLayout(sourceGroup);

    m_localRadio = new QRadioButton(tr("Local file (.m3u / .m3u8)"), sourceGroup);
    m_localRadio->setChecked(true);
    m_remoteRadio = new QRadioButton(tr("URL (http/https)"), sourceGroup);

    m_sourceField = new QLineEdit(sourceGroup);
    m_sourceField->setPlaceholderText(tr("Path to an .m3u or .m3u8 file..."));
    m_browseButton = new QPushButton(tr("Browse..."), sourceGroup);

    sourceLayout->addWidget(m_localRadio, 0, 0, 1, 3);
    sourceLayout->addWidget(m_remoteRadio, 1, 0, 1, 3);
    sourceLayout->addWidget(m_sourceField, 2, 0, 1, 2);
    sourceLayout->addWidget(m_browseButton, 2, 2);
    layout->addWidget(sourceGroup);

    auto *nameRow = new QGridLayout;
    nameRow->addWidget(new QLabel(tr("Name:"), this), 0, 0);
    m_nameField = new QLineEdit(this);
    m_nameField->setPlaceholderText(tr("Optional - defaults to the file name / host"));
    nameRow->addWidget(m_nameField, 0, 1);
    layout->addLayout(nameRow);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(tr("Add"));
    m_okButton->setDefault(true);
    layout->addWidget(buttons);

    connect(m_localRadio, &QRadioButton::toggled, this, [this](bool local) {
        m_browseButton->setEnabled(local);
        m_sourceField->setPlaceholderText(
            local ? tr("Path to an .m3u or .m3u8 file...")
                  : tr("https://example.com/playlist.m3u"));
        validateInput();
    });
    connect(m_browseButton, &QPushButton::clicked, this,
            &AddPlaylistDialog::browseForFile);
    connect(m_sourceField, &QLineEdit::textChanged, this,
            &AddPlaylistDialog::validateInput);
    connect(m_nameField, &QLineEdit::textChanged, this, [this]() {
        validateInput();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    validateInput();
}

Playlist::SourceType AddPlaylistDialog::sourceType() const
{
    return m_remoteRadio->isChecked() ? Playlist::SourceType::Remote
                                      : Playlist::SourceType::Local;
}

QString AddPlaylistDialog::sourceValue() const
{
    return m_sourceField->text().trimmed();
}

QString AddPlaylistDialog::playlistName() const
{
    const QString given = m_nameField->text().trimmed();
    if (!given.isEmpty())
        return given;
    if (sourceType() == Playlist::SourceType::Local)
        return QFileInfo(sourceValue()).completeBaseName();
    return QUrl(sourceValue()).host();
}

void AddPlaylistDialog::browseForFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open IPTV Playlist"), QString(),
        tr("IPTV playlists (*.m3u *.m3u8);;All files (*)"));
    if (file.isEmpty())
        return;
    m_sourceField->setText(file);
    if (m_nameField->text().trimmed().isEmpty())
        m_nameField->setText(QFileInfo(file).completeBaseName());
}

void AddPlaylistDialog::validateInput()
{
    const QString value = m_sourceField->text().trimmed();
    bool ok = false;
    if (m_remoteRadio->isChecked()) {
        const QUrl url(value);
        ok = url.isValid() && !url.host().isEmpty() &&
             (url.scheme() == QStringLiteral("http") ||
              url.scheme() == QStringLiteral("https"));
    } else {
        const QFileInfo info(value);
        ok = info.exists();
    }
    m_okButton->setEnabled(ok);
}