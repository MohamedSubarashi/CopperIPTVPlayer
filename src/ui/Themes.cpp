#include "ui/Themes.h"

#include <QApplication>
#include <QFile>

namespace {

QString readStyleSheet(const QString &resourcePath, bool &ok)
{
    QFile f(resourcePath);
    ok = f.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!ok)
        return QString();
    const QString content = QString::fromUtf8(f.readAll());
    f.close();
    return content;
}

} // namespace

namespace Themes {

void apply(Kind kind, bool reload)
{
    Q_UNUSED(reload)
    QApplication *app = qApp;
    if (!app)
        return;

    const bool dark = kind == Kind::Dark;
    const QString path = dark ? QStringLiteral(":/styles/dark.qss")
                              : QStringLiteral(":/styles/light.qss");
    bool ok = false;
    const QString css = readStyleSheet(path, ok);
    if (ok)
        app->setStyleSheet(css);
}

} // namespace Themes