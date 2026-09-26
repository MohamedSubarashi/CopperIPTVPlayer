#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "models/Channel.h"

// Tolerant M3U/M3U8 playlist parser for IPTV. Pure string processing: no I/O,
// no widgets, safe to run on a worker thread. Never throws; malformed entries
// are skipped and reported in the result so callers can log diagnostics.
class M3UParser
{
public:
    struct Result
    {
        QVector<Channel> channels;
        QVector<QString> warnings;
        int skippedLines = 0;
    };

    static Result parse(const QString &content);

private:
    struct Pending
    {
        QString name;
        QHash<QString, QString> attrs;
        QString groupOverride;
    };

    static QString nameAfterLastUnquotedComma(const QString &text, bool *ok);
    static void splitNameFromInf(QString *attributesText, QString *name);
    static QHash<QString, QString> parseAttributes(const QString &text);
    static void applyAttributes(Channel *channel, const QHash<QString, QString> &attrs);
    static QString deriveNameFromUrl(const QString &url);
    static Pending parseExtinf(const QString &line);
};