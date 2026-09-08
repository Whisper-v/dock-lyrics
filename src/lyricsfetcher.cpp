#include "lyricsfetcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

QString urlQueryEncode(const QString &s)
{
    return QUrl::toPercentEncoding(s);
}

LyricsFetcher::LyricsFetcher(QObject *parent)
    : QObject(parent)
{
}

static QString canonicalKey(const QString &title, const QString &artist)
{
    QString t = title.trimmed();
    QString a = artist.trimmed();
    // strip leading articles like "The " for better local matching
    QStringList words = t.split(' ', Qt::SkipEmptyParts);
    if (words.size() > 1) {
        const QString &first = words.first().toLower();
        if (first == "the" || first == "a" || first == "an")
            words.removeFirst();
    }
    t = words.join(' ').toLower();
    a = a.toLower();
    return a.isEmpty() ? t : a + " - " + t;
}

void LyricsFetcher::requestLyrics(const QString &key,
                                  const QString &title,
                                  const QString &artist,
                                  const QString &localUrlHint)
{
    // Kill any in-flight request.
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }

    m_pending = { key, title, artist };

    const QString local = findLocalLrc(title, artist, localUrlHint);
    if (!local.isEmpty()) {
        QFile f(local);
        if (f.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(f.readAll());
            emit lyricsReady(key, QStringLiteral("local"), content);
            return;
        }
    }

    startOnlineSearch(m_pending);
}

QString LyricsFetcher::findLocalLrc(const QString &title, const QString &artist,
                                    const QString &localUrlHint) const
{
    // 1. sidecar .lrc next to the playing local file (file:// URL from MPRIS)
    if (localUrlHint.startsWith(QStringLiteral("file://"))) {
        const QString audioPath = QUrl(localUrlHint).toLocalFile();
        if (!audioPath.isEmpty()) {
            const QFileInfo audio(audioPath);
            const QString sidecar = audio.absolutePath() + QLatin1Char('/')
                                    + audio.completeBaseName() + QStringLiteral(".lrc");
            if (QFileInfo::exists(sidecar))
                return sidecar;
        }
    }

    // 2. search common music directories for common file name patterns
    QStringList dirs;
    const QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (!musicDir.isEmpty())
        dirs << musicDir;
    const QString home = QDir::homePath();
    dirs << home + QStringLiteral("/Music")
         << home + QStringLiteral("/音乐")
         << home + QStringLiteral("/Music/Lyrics")
         << home + QStringLiteral("/音乐/歌词");

    const QString primaryArtist = artist.split(QRegularExpression("[/、&,]"), Qt::SkipEmptyParts)
                                      .value(0).trimmed();
    QStringList names;
    if (!title.isEmpty()) {
        names << title + QStringLiteral(".lrc");
        if (!primaryArtist.isEmpty()) {
            names << primaryArtist + QStringLiteral(" - ") + title + QStringLiteral(".lrc")
                  << title + QStringLiteral(" - ") + primaryArtist + QStringLiteral(".lrc")
                  << primaryArtist + QStringLiteral("-") + title + QStringLiteral(".lrc");
        }
    }

    for (const QString &dir : dirs) {
        for (const QString &name : names) {
            const QString p = dir + QLatin1Char('/') + name;
            if (QFileInfo::exists(p))
                return p;
        }
    }
    return QString();
}

void LyricsFetcher::startOnlineSearch(const Pending &p)
{
    QNetworkRequest req;
    const QString query = p.artist.trimmed().isEmpty()
                              ? p.title
                              : p.title + QLatin1Char(' ') + p.artist;
    req.setUrl(QUrl(QStringLiteral("https://music.163.com/api/search/get?s=%1&type=1&limit=8&offset=0")
                        .arg(urlQueryEncode(query))));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"));
    req.setRawHeader("Referer", "https://music.163.com/");
    req.setTransferTimeout(8000);

    m_currentReply = m_nam.get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &LyricsFetcher::onSearchFinished);
}

void LyricsFetcher::onSearchFinished()
{
    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    if (!reply)
        return;

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError || data.isEmpty()) {
        emit fetchFailed(m_pending.key);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit fetchFailed(m_pending.key);
        return;
    }
    const QJsonObject result = doc.object().value(QStringLiteral("result")).toObject();
    const QJsonArray songs = result.value(QStringLiteral("songs")).toArray();
    if (songs.isEmpty()) {
        emit fetchFailed(m_pending.key);
        return;
    }

    // Pick the best match: prefer exact title, then the artist being mentioned.
    const QString wantTitle = m_pending.title.trimmed();
    const QStringList wantArtists = m_pending.artist
                                        .split(QRegularExpression("[/、&,]"), Qt::SkipEmptyParts);
    qint64 bestId = 0;
    int bestScore = -1000;
    for (const QJsonValue &v : songs) {
        const QJsonObject song = v.toObject();
        const QString name = song.value(QStringLiteral("name")).toString();
        int score = 0;
        if (!wantTitle.isEmpty()) {
            if (name.compare(wantTitle, Qt::CaseInsensitive) == 0)
                score += 100;
            else if (name.contains(wantTitle, Qt::CaseInsensitive))
                score += 40;
        }
        const QJsonArray artists = song.value(QStringLiteral("artists")).toArray();
        QStringList got;
        for (const QJsonValue &a : artists)
            got << a.toObject().value(QStringLiteral("name")).toString();
        for (const QString &w : wantArtists) {
            const QString ww = w.trimmed();
            if (ww.isEmpty())
                continue;
            bool any = false;
            for (const QString &g : got) {
                if (g.contains(ww, Qt::CaseInsensitive) || ww.contains(g, Qt::CaseInsensitive)) {
                    any = true;
                    break;
                }
            }
            if (any)
                score += 15;
            else
                score -= 3;
        }
        if (score > bestScore) {
            bestScore = score;
            bestId = song.value(QStringLiteral("id")).toVariant().toLongLong();
        }
    }

    if (bestId <= 0) {
        emit fetchFailed(m_pending.key);
        return;
    }
    startLyricRequest(bestId);
}

void LyricsFetcher::startLyricRequest(qint64 songId)
{
    QNetworkRequest req;
    req.setUrl(QUrl(QStringLiteral("https://music.163.com/api/song/lyric?id=%1&lv=1&kv=1&tv=-1")
                        .arg(songId)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"));
    req.setRawHeader("Referer", "https://music.163.com/");
    req.setTransferTimeout(8000);

    m_currentReply = m_nam.get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &LyricsFetcher::onLyricFinished);
}

void LyricsFetcher::onLyricFinished()
{
    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    if (!reply)
        return;

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError || data.isEmpty()) {
        emit fetchFailed(m_pending.key);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit fetchFailed(m_pending.key);
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonObject lrcObj = root.value(QStringLiteral("lrc")).toObject();
    QString lrcText = lrcObj.value(QStringLiteral("lyric")).toString();
    if (lrcText.isEmpty()) {
        // fall back to translated lyric if any
        lrcText = root.value(QStringLiteral("tlyric")).toObject()
                      .value(QStringLiteral("lyric")).toString();
    }

    const QString key = m_pending.key;
    if (lrcText.isEmpty()) {
        emit fetchFailed(key);
        return;
    }
    emit lyricsReady(key, QStringLiteral("online"), lrcText);
}
