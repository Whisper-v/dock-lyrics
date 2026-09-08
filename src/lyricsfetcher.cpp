#include "lyricsfetcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

QString urlQueryEncode(const QString &s)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

LyricsFetcher::LyricsFetcher(QObject *parent)
    : QObject(parent)
{
}

void LyricsFetcher::requestLyrics(const QString &key,
                                  const QString &title,
                                  const QString &artist,
                                  const QString &localUrlHint)
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
    m_stage = Stage::Idle;

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

    startNeteaseSearch();
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
         << home + QStringLiteral("/音乐/歌词")
         // common download folders used by Chinese players
         << home + QStringLiteral("/Music/QQMusic")
         << home + QStringLiteral("/Music/QQ音乐")
         << home + QStringLiteral("/Music/qqmusic")
         << home + QStringLiteral("/Music/网易云音乐")
         << home + QStringLiteral("/Music/CloudMusic")
         << home + QStringLiteral("/Music/酷狗音乐")
         << home + QStringLiteral("/音乐/QQ音乐")
         << home + QStringLiteral("/音乐/网易云音乐");

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

void LyricsFetcher::doGet(const QUrl &url, Stage stage)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"));
    req.setRawHeader("Referer", "https://music.163.com/");
    req.setTransferTimeout(8000);

    m_stage = stage;
    m_currentReply = m_nam.get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &LyricsFetcher::onReplyFinished);
}

void LyricsFetcher::startNeteaseSearch()
{
    const QString query = m_pending.artist.trimmed().isEmpty()
                              ? m_pending.title
                              : m_pending.title + QLatin1Char(' ') + m_pending.artist;
    const QUrl url(QStringLiteral("https://music.163.com/api/search/get?s=%1&type=1&limit=10&offset=0")
                       .arg(urlQueryEncode(query)));
    doGet(url, Stage::NeteaseSearch);
}

void LyricsFetcher::startNeteaseLyric(qint64 songId)
{
    const QUrl url(QStringLiteral("https://music.163.com/api/song/lyric?id=%1&lv=1&kv=1&tv=-1")
                       .arg(songId));
    doGet(url, Stage::NeteaseLyric);
}

void LyricsFetcher::startLrclibSearch()
{
    QUrlQuery q;
    if (!m_pending.title.trimmed().isEmpty())
        q.addQueryItem(QStringLiteral("track_name"), m_pending.title.trimmed());
    if (!m_pending.artist.trimmed().isEmpty())
        q.addQueryItem(QStringLiteral("artist_name"), m_pending.artist.trimmed());
    QUrl url(QStringLiteral("https://lrclib.net/api/search"));
    url.setQuery(q);
    doGet(url, Stage::LrclibSearch);
}

void LyricsFetcher::onReplyFinished()
{
    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    if (!reply)
        return;

    const QByteArray data = reply->readAll();
    const bool netError = (reply->error() != QNetworkReply::NoError);
    reply->deleteLater();

    const QString key = m_pending.key;

    switch (m_stage) {
    case Stage::NeteaseSearch: {
        if (netError || data.isEmpty()) {
            // Network trouble: still give the fallback database a chance.
            startLrclibSearch();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            startLrclibSearch();
            return;
        }
        const QJsonObject result = doc.object().value(QStringLiteral("result")).toObject();
        const QJsonArray songs = result.value(QStringLiteral("songs")).toArray();
        if (songs.isEmpty()) {
            startLrclibSearch();
            return;
        }

        // Pick the best candidate, but only accept a *confident* match so we do
        // not show lyrics of an unrelated cover/remix song.
        const QString wantTitle = m_pending.title.trimmed();
        const QStringList wantArtists = m_pending.artist
                                            .split(QRegularExpression("[/、&,]"), Qt::SkipEmptyParts);
        bool accept = false;
        qint64 bestId = 0;
        int bestScore = -1000;
        for (const QJsonValue &v : songs) {
            const QJsonObject song = v.toObject();
            const QString name = song.value(QStringLiteral("name")).toString();
            int score = 0;
            bool titleExact = false;
            bool titleContained = false;
            if (!wantTitle.isEmpty()) {
                if (name.compare(wantTitle, Qt::CaseInsensitive) == 0) {
                    score += 100;
                    titleExact = true;
                } else if (name.contains(wantTitle, Qt::CaseInsensitive)) {
                    score += 40;
                    titleContained = true;
                }
            }
            bool artistOk = wantArtists.isEmpty();
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
                if (any) {
                    artistOk = true;
                    score += 15;
                } else {
                    score -= 3;
                }
            }
            if (titleExact || (titleContained && artistOk))
                accept = true;
            if (score > bestScore) {
                bestScore = score;
                bestId = song.value(QStringLiteral("id")).toVariant().toLongLong();
            }
        }

        if (accept && bestId > 0) {
            startNeteaseLyric(bestId);
        } else {
            qWarning() << "[dock-lyrics] netease: no confident match, fallback to lrclib";
            startLrclibSearch();
        }
        break;
    }

    case Stage::NeteaseLyric: {
        QString lrcText;
        if (!netError && !data.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                const QJsonObject root = doc.object();
                const QJsonObject lrcObj = root.value(QStringLiteral("lrc")).toObject();
                lrcText = lrcObj.value(QStringLiteral("lyric")).toString();
                if (lrcText.isEmpty())
                    lrcText = root.value(QStringLiteral("tlyric")).toObject()
                                  .value(QStringLiteral("lyric")).toString();
            }
        }
        if (lrcText.isEmpty()) {
            qWarning() << "[dock-lyrics] netease: empty lyric, fallback to lrclib";
            startLrclibSearch();
            return;
        }
        emit lyricsReady(key, QStringLiteral("netease"), lrcText);
        break;
    }

    case Stage::LrclibSearch: {
        if (netError || data.isEmpty()) {
            emitFailure(key);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) {
            emitFailure(key);
            return;
        }
        const QJsonArray arr = doc.array();
        if (arr.isEmpty()) {
            emitFailure(key);
            return;
        }

        // LRCLIB returns full records (including synced/plain lyrics) in search
        // results, so no extra per-song request is needed.
        const QString wantTitle = m_pending.title.trimmed();
        const QStringList wantArtists = m_pending.artist
                                            .split(QRegularExpression("[/、&,]"), Qt::SkipEmptyParts);
        int bestScore = -1000;
        int bestIdx = -1;
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonObject obj = arr.at(i).toObject();
            if (obj.value(QStringLiteral("instrumental")).toBool())
                continue;
            const QString track = obj.value(QStringLiteral("trackName")).toString();
            const QString artist = obj.value(QStringLiteral("artistName")).toString();
            const QString synced = obj.value(QStringLiteral("syncedLyrics")).toString();
            const QString plain = obj.value(QStringLiteral("plainLyrics")).toString();
            if (synced.isEmpty() && plain.isEmpty())
                continue;

            int score = 0;
            bool titleExact = false;
            bool titleContained = false;
            if (!wantTitle.isEmpty()) {
                if (track.compare(wantTitle, Qt::CaseInsensitive) == 0) {
                    score += 100;
                    titleExact = true;
                } else if (track.contains(wantTitle, Qt::CaseInsensitive)) {
                    score += 40;
                    titleContained = true;
                }
            }
            bool artistOk = wantArtists.isEmpty();
            for (const QString &w : wantArtists) {
                const QString ww = w.trimmed();
                if (ww.isEmpty())
                    continue;
                if (artist.contains(ww, Qt::CaseInsensitive) || ww.contains(artist, Qt::CaseInsensitive)) {
                    artistOk = true;
                    score += 15;
                }
            }
            if (!synced.isEmpty())
                score += 10;
            if (!(titleExact || (titleContained && artistOk)))
                continue;
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }

        if (bestIdx < 0) {
            qWarning() << "[dock-lyrics] lrclib: no match for"
                       << m_pending.artist << "-" << m_pending.title;
            emitFailure(key);
            return;
        }
        const QJsonObject best = arr.at(bestIdx).toObject();
        const QString text = best.value(QStringLiteral("syncedLyrics")).toString();
        const QString fallback = best.value(QStringLiteral("plainLyrics")).toString();
        const QString finalText = text.isEmpty() ? fallback : text;
        if (finalText.isEmpty()) {
            emitFailure(key);
            return;
        }
        qWarning() << "[dock-lyrics] lrclib match:"
                   << best.value(QStringLiteral("trackName")).toString()
                   << "by" << best.value(QStringLiteral("artistName")).toString();
        emit lyricsReady(key, QStringLiteral("lrclib"), finalText);
        break;
    }

    case Stage::Idle:
        break;
    }
}

void LyricsFetcher::emitFailure(const QString &key)
{
    qWarning() << "[dock-lyrics] all lyric sources failed for key" << key;
    emit fetchFailed(key);
}
