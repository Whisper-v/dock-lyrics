#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace lrc {

struct Line
{
    qint64 timeMs = -1; // -1 means untimed (plain text)
    QString text;
};

// Parse an LRC file. Returns timed lines (metadata lines filtered out).
// If the content has no time tags, plainLines is filled instead and the
// returned vector only contains lines with timeMs == -1.
QVector<Line> parseLrc(const QString &lrcContent, QStringList *plainLines = nullptr);

// Keep only lines that look like real lyrics (drop composer/arranger etc.)
bool isMetaText(const QString &text);

} // namespace lrc
