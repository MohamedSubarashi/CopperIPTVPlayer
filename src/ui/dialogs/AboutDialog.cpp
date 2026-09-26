#include "ui/dialogs/AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "core/AppInfo.h"
#include "core/Constants.h"

namespace {
QString buildInfoText()
{
    QString text = QStringLiteral("Version %1").arg(AppInfo::APP_VERSION);
#ifdef QT_DEBUG
    text += QStringLiteral(" (debug)");
#endif
    text += QStringLiteral("\nBuild: %1\nQt %2").arg(
        QString::fromUtf8(__DATE__), QString::fromUtf8(qVersion()));
    return text;
}
} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Copper IPTV Player"));
    setMinimumWidth(440);

    auto *layout = new QVBoxLayout(this);

    auto *appName = new QLabel(tr("Copper IPTV Player"), this);
    appName->setObjectName(QStringLiteral("aboutTitle"));
    layout->addWidget(appName, 0, Qt::AlignCenter);

    auto *version = new QLabel(buildInfoText(), this);
    version->setObjectName(QStringLiteral("aboutVersion"));
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);

    auto *description = new QLabel(
        tr("An M3U/M3U8 IPTV player for Windows built on Qt 6 with the Qt "
           "Multimedia backend."),
        this);
    description->setWordWrap(true);
    description->setObjectName(QStringLiteral("aboutDescription"));
    layout->addWidget(description);

    auto *details = new QTextBrowser(this);
    details->setOpenExternalLinks(true);
    details->setFrameShape(QFrame::NoFrame);

    QString html = tr("<p>Visit the project page for documentation, updates "
                      "and usage tips.</p>");
    if (!AppInfo::APP_GITHUB_URL.isEmpty())
        html += QStringLiteral("<p><a href=\"%1\">%1</a></p>").arg(AppInfo::APP_GITHUB_URL);
    if (!AppInfo::APP_DOCS_URL.isEmpty())
        html += QStringLiteral("<p><a href=\"%1\">%1</a></p>").arg(AppInfo::APP_DOCS_URL);
    details->setHtml(html);
    details->setMinimumHeight(90);
    layout->addWidget(details);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::reject);
}