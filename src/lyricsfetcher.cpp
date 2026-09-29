#include "lyricsfetcher.h"

#include <QByteArray>
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

// QQ Music's fcgi endpoints wrap the JSON payload in a JSONP callback
// (e.g. "MusicJsonCallback({...})"). Strip everything outside the braces.
static QJsonDocument parseJsonp(const QByteArray &data)
{
    const int start = data.indexOf('{');
    const int end = data.lastIndexOf('}');
    if (start >= 0 && end > start)
        return QJsonDocument::fromJson(data.mid(start, end - start + 1));
    return QJsonDocument::fromJson(data);
}

// Normalize a file/artist/title name for fuzzy matching: drop spaces,
// separators and punctuation (e.g. "周杰伦 - .水手怕水" -> "周杰伦水手怕水").
static QString normalizeLrcName(const QString &s)
{
    QString n = s;
    n.remove(QRegularExpression(QStringLiteral(
        "[\\s\\-._《》〈〉「」『』【】（）()\\[\\]\\{\\}、，,。!！?？:：/\\\\]+")));
    return n.toLower();
}

// Strip common version/quality suffixes like "(live)" / "(伴奏)" / "[demo]"
// from a title so online searches can match the plain studio version.
static QString lyricSearchTitle(const QString &title)
{
    QString t = title.trimmed();
    t.remove(QRegularExpression(QStringLiteral("\\s*\\([^)]*\\)\\s*")));
    t.remove(QRegularExpression(QStringLiteral("\\s*\\[[^\\]]*\\]\\s*")));
    return t.trimmed();
}

// Depth-limited recursive search for a .lrc whose normalized name contains
// the normalized title (and artist when given).
static QString findLrcRecursive(const QString &dirPath, const QString &normTitle,
                                const QString &normArtist, int depth)
{
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &e : entries) {
        if (e.isDir()) {
            if (depth > 0) {
                const QString found = findLrcRecursive(e.absoluteFilePath(), normTitle,
                                                       normArtist, depth - 1);
                if (!found.isEmpty())
                    return found;
            }
        } else if (e.suffix().compare(QLatin1String("lrc"), Qt::CaseInsensitive) == 0) {
            const QString base = normalizeLrcName(e.completeBaseName());
            if (!base.isEmpty() && base.contains(normTitle)) {
                if (normArtist.isEmpty() || base.contains(normArtist))
                    return e.absoluteFilePath();
            }
        }
    }
    return QString();
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

    // Local .lrc loading is disabled by request: always walk the online
    // sources (NetEase -> Kugou -> LRCLIB).
    Q_UNUSED(localUrlHint);
    // For matching, drop version suffixes (live / instrumental / demo ...),
    // while the raw title stays in the key used for caching.
    m_pending.cleanTitle = lyricSearchTitle(title);
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

    // 3. recursive fuzzy search under the common music roots, so lyrics stored
    //    in deep sub-folders (e.g. ~/Music/歌手/专辑/歌手 - .歌名.lrc) are found.
    //    This also covers players that do not report xesam:url (e.g. DeepinMusic).
    const QString normTitle = normalizeLrcName(title);
    if (!normTitle.isEmpty()) {
        const QString normArtist = normalizeLrcName(primaryArtist);
        QStringList roots;
        if (!musicDir.isEmpty())
            roots << musicDir;
        roots << home + QStringLiteral("/Music") << home + QStringLiteral("/音乐");
        for (const QString &root : roots) {
            if (QDir(root).exists()) {
                const QString found = findLrcRecursive(root, normTitle, normArtist, 4);
                if (!found.isEmpty()) {
                    qWarning() << "[dock-lyrics] local lrc found:" << found;
                    return found;
                }
            }
        }
    }
    return QString();
}

void LyricsFetcher::doGet(const QUrl &url, Stage stage)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"));
    QString referer = QStringLiteral("https://music.163.com/");
    if (stage == Stage::KugouSearch || stage == Stage::KugouKrcSearch
        || stage == Stage::KugouDownload)
        referer = QStringLiteral("https://www.kugou.com/");
    else if (stage == Stage::QqSearch || stage == Stage::QqLyric)
        referer = QStringLiteral("https://y.qq.com/");
    req.setRawHeader("Referer", referer.toUtf8());
    req.setTransferTimeout(8000);

    m_stage = stage;
    m_currentReply = m_nam.get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &LyricsFetcher::onReplyFinished);
}

void LyricsFetcher::startNeteaseSearch()
{
    const QString query = m_pending.artist.trimmed().isEmpty()
                              ? m_pending.cleanTitle
                              : m_pending.cleanTitle + QLatin1Char(' ') + m_pending.artist;
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

void LyricsFetcher::startKugouSearch()
{
    const QString query = m_pending.artist.trimmed().isEmpty()
                              ? m_pending.cleanTitle
                              : m_pending.cleanTitle + QLatin1Char(' ') + m_pending.artist;
    const QUrl url(QStringLiteral("https://songsearch.kugou.com/song_search_v2?keyword=%1&page=1&pagesize=10")
                       .arg(urlQueryEncode(query)));
    doGet(url, Stage::KugouSearch);
}

void LyricsFetcher::startKugouKrcSearch(const QString &hash, const QString &songName)
{
    QUrlQuery q;
    if (!songName.trimmed().isEmpty())
        q.addQueryItem(QStringLiteral("keyword"), songName.trimmed());
    if (!hash.isEmpty())
        q.addQueryItem(QStringLiteral("hash"), hash);
    q.addQueryItem(QStringLiteral("ver"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("man"), QStringLiteral("yes"));
    q.addQueryItem(QStringLiteral("client"), QStringLiteral("mobi"));
    QUrl url(QStringLiteral("https://krcs.kugou.com/search"));
    url.setQuery(q);
    doGet(url, Stage::KugouKrcSearch);
}

void LyricsFetcher::startKugouDownload(const QString &id, const QString &accessKey)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("ver"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("client"), QStringLiteral("pc"));
    if (!id.isEmpty())
        q.addQueryItem(QStringLiteral("id"), id);
    if (!accessKey.isEmpty())
        q.addQueryItem(QStringLiteral("accesskey"), accessKey);
    q.addQueryItem(QStringLiteral("fmt"), QStringLiteral("lrc"));
    q.addQueryItem(QStringLiteral("charset"), QStringLiteral("utf8"));
    QUrl url(QStringLiteral("https://lyrics.kugou.com/download"));
    url.setQuery(q);
    doGet(url, Stage::KugouDownload);
}

void LyricsFetcher::startQqSearch()
{
    const QString query = m_pending.artist.trimmed().isEmpty()
                              ? m_pending.cleanTitle
                              : m_pending.cleanTitle + QLatin1Char(' ') + m_pending.artist;
    const QUrl url(QStringLiteral("https://c.y.qq.com/soso/fcgi-bin/client_search_cp?p=1&n=10&format=json&w=")
                       .arg(urlQueryEncode(query)));
    doGet(url, Stage::QqSearch);
}

void LyricsFetcher::startQqLyric(const QString &songMid)
{
    const QUrl url(QStringLiteral("https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg?songmid=&format=json&nobase64=0")
                       .arg(songMid));
    doGet(url, Stage::QqLyric);
}

void LyricsFetcher::startLrclibSearch()
{
    QUrlQuery q;
    if (!m_pending.cleanTitle.trimmed().isEmpty())
        q.addQueryItem(QStringLiteral("track_name"), m_pending.cleanTitle.trimmed());
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
            // Network trouble: give Kugou / LRCLIB fallback databases a chance.
            startKugouSearch();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            startKugouSearch();
            return;
        }
        const QJsonObject result = doc.object().value(QStringLiteral("result")).toObject();
        const QJsonArray songs = result.value(QStringLiteral("songs")).toArray();
        if (songs.isEmpty()) {
            startKugouSearch();
            return;
        }

        // Pick the best candidate, but only accept a *confident* match so we do
        // not show lyrics of an unrelated cover/remix song.
        const QString wantTitle = m_pending.cleanTitle;
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
            qWarning() << "[dock-lyrics] netease: no confident match, try kugou";
            startKugouSearch();
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
            qWarning() << "[dock-lyrics] netease: empty lyric, try kugou";
            startKugouSearch();
            return;
        }
        emit lyricsReady(key, QStringLiteral("netease"), lrcText);
        break;
    }

    case Stage::KugouSearch: {
        if (netError || data.isEmpty()) {
            startQqSearch();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            startQqSearch();
            return;
        }
        const QJsonArray lists = doc.object().value(QStringLiteral("data"))
                                       .toObject().value(QStringLiteral("lists")).toArray();
        if (lists.isEmpty()) {
            qWarning() << "[dock-lyrics] kugou: empty search result, try qq";
            startQqSearch();
            return;
        }
        const QString wantTitle = m_pending.cleanTitle;
        const QStringList wantArtists = m_pending.artist
                                            .split(QRegularExpression("[/、&,]"), Qt::SkipEmptyParts);
        QString bestHash, bestName;
        int bestScore = -1000;
        for (const QJsonValue &v : lists) {
            const QJsonObject song = v.toObject();
            const QString rawName = song.value(QStringLiteral("SongName")).toString();
            const QString singer = song.value(QStringLiteral("SingerName")).toString();
            // strip suffixes like " (Live)" / " (伴奏)" for a cleaner match
            QString base = rawName;
            base.remove(QRegularExpression(QStringLiteral("\\s*\\(.*\\)\\s*")));
            base = base.trimmed();
            int score = 0;
            if (!wantTitle.isEmpty()) {
                if (base.compare(wantTitle, Qt::CaseInsensitive) == 0)
                    score += 100;
                else if (base.contains(wantTitle, Qt::CaseInsensitive))
                    score += 40;
                else if (rawName.contains(wantTitle, Qt::CaseInsensitive))
                    score += 15;
            }
            bool artistOk = wantArtists.isEmpty();
            for (const QString &w : wantArtists) {
                const QString ww = w.trimmed();
                if (ww.isEmpty())
                    continue;
                if (singer.contains(ww, Qt::CaseInsensitive)) {
                    artistOk = true;
                    score += 15;
                    break;
                }
            }
            if (!artistOk)
                score -= 10;
            if (score > bestScore) {
                bestScore = score;
                bestHash = song.value(QStringLiteral("FileHash")).toString();
                bestName = rawName;
            }
        }
        if (bestScore >= 50 && !bestHash.isEmpty()) {
            qWarning() << "[dock-lyrics] kugou: candidate" << bestName << "hash" << bestHash;
            startKugouKrcSearch(bestHash, bestName);
        } else {
            qWarning() << "[dock-lyrics] kugou: no confident match, try qq";
            startQqSearch();
        }
        break;
    }

    case Stage::KugouKrcSearch: {
        if (netError || data.isEmpty()) {
            startQqSearch();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            startQqSearch();
            return;
        }
        const QJsonArray candidates = doc.object().value(QStringLiteral("candidates")).toArray();
        if (candidates.isEmpty()) {
            qWarning() << "[dock-lyrics] kugou: no krc candidate, try qq";
            startQqSearch();
            return;
        }
        const QJsonObject first = candidates.first().toObject();
        const QString id = first.value(QStringLiteral("id")).toString();
        const QString accessKey = first.value(QStringLiteral("accesskey")).toString();
        if (id.isEmpty() || accessKey.isEmpty()) {
            startQqSearch();
            return;
        }
        startKugouDownload(id, accessKey);
        break;
    }

    case Stage::KugouDownload: {
        if (netError || data.isEmpty()) {
            startQqSearch();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            startQqSearch();
            return;
        }
        const QString b64 = doc.object().value(QStringLiteral("content")).toString();
        if (b64.isEmpty()) {
            qWarning() << "[dock-lyrics] kugou: empty lyric content, try qq";
            startQqSearch();
            return;
        }
        QString lrc = QString::fromUtf8(QByteArray::fromBase64(b64.toUtf8()));
        if (lrc.startsWith(QChar(0xFEFF)))
            lrc.remove(0, 1);
        if (lrc.trimmed().isEmpty()) {
            startQqSearch();
            return;
        }
        qWarning() << "[dock-lyrics] kugou lyric ready, lines=" << lrc.split(QLatin1Char('\n')).size();
        emit lyricsReady(key, QStringLiteral("kugou"), lrc);
        break;
    }

    case Stage::QqSearch: {
        if (netError || data.isEmpty()) {
            startLrclibSearch();
            return;
        }
        const QJsonDocument doc = parseJsonp(data);
        if (!doc.isObject()) {
            startLrclibSearch();
            return;
        }
        const QJsonArray list = doc.object().value(QStringLiteral("data")).toObject()
                                       .value(QStringLiteral("song")).toObject()
                                       .value(QStringLiteral("list")).toArray();
        if (list.isEmpty()) {
            qWarning() << "[dock-lyrics] qq: empty search result, fallback to lrclib";
            startLrclibSearch();
            return;
        }
        const QString wantTitle = m_pending.cleanTitle;
        const QStringList wantArtists = m_pending.artist
                                            .split(QRegularExpression("[/、&,]"), Qt::SkipEmptyParts);
        QString bestMid;
        int bestScore = -1000;
        for (const QJsonValue &v : list) {
            const QJsonObject song = v.toObject();
            const QString name = song.value(QStringLiteral("songname")).toString();
            QString base = name;
            base.remove(QRegularExpression(QStringLiteral("\\s*\\(.*\\)\\s*")));
            base = base.trimmed();
            int score = 0;
            bool titleExact = false;
            bool titleContained = false;
            if (!wantTitle.isEmpty()) {
                if (base.compare(wantTitle, Qt::CaseInsensitive) == 0) {
                    score += 100;
                    titleExact = true;
                } else if (base.contains(wantTitle, Qt::CaseInsensitive)) {
                    score += 40;
                    titleContained = true;
                } else if (name.contains(wantTitle, Qt::CaseInsensitive)) {
                    score += 15;
                }
            }
            bool artistOk = wantArtists.isEmpty();
            const QJsonArray singers = song.value(QStringLiteral("singer")).toArray();
            QStringList got;
            for (const QJsonValue &a : singers)
                got << a.toObject().value(QStringLiteral("name")).toString();
            for (const QString &w : wantArtists) {
                const QString ww = w.trimmed();
                if (ww.isEmpty())
                    continue;
                for (const QString &g : got) {
                    if (g.contains(ww, Qt::CaseInsensitive) || ww.contains(g, Qt::CaseInsensitive)) {
                        artistOk = true;
                        score += 15;
                        break;
                    }
                }
            }
            if (!artistOk)
                score -= 10;
            if (!(titleExact || (titleContained && artistOk)))
                continue;
            if (score > bestScore) {
                bestScore = score;
                bestMid = song.value(QStringLiteral("songmid")).toString();
            }
        }
        if (bestScore >= 50 && !bestMid.isEmpty()) {
            m_pending.qqSongMid = bestMid;
            qWarning() << "[dock-lyrics] qq: candidate mid" << bestMid;
            startQqLyric(bestMid);
        } else {
            qWarning() << "[dock-lyrics] qq: no confident match, fallback to lrclib";
            startLrclibSearch();
        }
        break;
    }

    case Stage::QqLyric: {
        if (netError || data.isEmpty()) {
            startLrclibSearch();
            return;
        }
        const QJsonDocument doc = parseJsonp(data);
        if (!doc.isObject()) {
            startLrclibSearch();
            return;
        }
        const QString b64 = doc.object().value(QStringLiteral("lyric")).toString();
        if (b64.isEmpty()) {
            qWarning() << "[dock-lyrics] qq: empty lyric content, fallback to lrclib";
            startLrclibSearch();
            return;
        }
        QString lrc = QString::fromUtf8(QByteArray::fromBase64(b64.toUtf8()));
        if (lrc.startsWith(QChar(0xFEFF)))
            lrc.remove(0, 1);
        if (lrc.trimmed().isEmpty()) {
            startLrclibSearch();
            return;
        }
        qWarning() << "[dock-lyrics] qq lyric ready, lines=" << lrc.split(QLatin1Char('\n')).size();
        emit lyricsReady(key, QStringLiteral("qq"), lrc);
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
        const QString wantTitle = m_pending.cleanTitle;
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
