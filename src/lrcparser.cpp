#include "lrcparser.h"

#include <QRegularExpression>
#include <algorithm>

namespace lrc {

static const char *kMetaWords[] = {
    "作词", "作曲", "编曲", "制作人", "制作", "混音", "母带", "录音",
    "和声", "和音", "吉他", "贝斯", "鼓", "键盘", "钢琴", "弦乐",
    "监制", "出品", "发行", "企划", "统筹", "封面", "配唱", "人声",
    "原唱", "翻唱", "词曲", "OP", "SP", "编舞", "导演", "摄影",
    nullptr
};

bool isMetaText(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return true;
    for (int i = 0; kMetaWords[i]; ++i) {
        const QString w = QString::fromUtf8(kMetaWords[i]);
        // e.g. "作词 : xxx", "作词:xxx", "吉他：xxx"
        if (t.startsWith(w)) {
            if (t.size() <= w.size())
                return true;
            const ushort u = t.at(w.size()).unicode();
            // ':' 0x3A  '：' 0xFF1A  ' ' 0x20  '.' 0x2E
            if (u == 0x3A || u == 0xFF1A || u == 0x20 || u == 0x2E)
                return true;
        }
    }
    return false;
}

QVector<Line> parseLrc(const QString &lrcContent, QStringList *plainLines)
{
    QVector<Line> result;
    QStringList plain;
    const QStringList rawLines = lrcContent.split(QRegularExpression("[\r\n]+"),
                                                  Qt::SkipEmptyParts);
    static const QRegularExpression tagRe("\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]");

    bool anyTimed = false;
    for (const QString &raw : rawLines) {
        QString text = raw;
        QVector<qint64> times;

        int pos = 0;
        QRegularExpressionMatchIterator it = tagRe.globalMatch(raw);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            times.append(m.captured(1).toLongLong() * 60000
                         + m.captured(2).toLongLong() * 1000
                         + (m.captured(3).isEmpty() ? 0
                            : QString(m.captured(3)).leftJustified(3, '0').toLongLong()));
            pos = m.capturedEnd();
        }
        if (!times.isEmpty())
            text = raw.mid(pos).trimmed();
        if (text.isEmpty())
            continue;

        if (times.isEmpty()) {
            plain.append(text);
            continue;
        }
        anyTimed = true;
        if (isMetaText(text))
            continue;
        for (qint64 t : times)
            result.append({t, text});
    }

    if (plainLines)
        *plainLines = plain;

    if (!anyTimed) {
        // Not a synced LRC: expose it as untimed plain lines.
        result.clear();
        QVector<Line> untimed;
        for (const QString &s : plain) {
            if (!isMetaText(s))
                untimed.append({-1, s});
        }
        return untimed;
    }

    std::sort(result.begin(), result.end(),
              [](const Line &a, const Line &b) { return a.timeMs < b.timeMs; });
    return result;
}

} // namespace lrc
