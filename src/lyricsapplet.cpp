#include "lyricsapplet.h"

#include <pluginfactory.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QMetaType>
#include <QVariant>
#include <QVariantMap>

#include "lyricsfetcher.h"
#include "qdbusutil.h"

static const QString kMprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
static const QString kPlayerPath = QStringLiteral("/org/mpris/MediaPlayer2");
static const QString kPlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");

static QStringList toStringList(const QVariant &v)
{
    if (v.typeId() == QMetaType::QString)
        return QStringList() << v.toString();
    return v.toStringList();
}

// Some players (Chromium-based web players, e.g. QQ Music) expose the page URL
// or SPA route as the media title when no real MediaSession metadata is set.
// Treat such strings as "no song title" so we don't fetch lyrics for garbage.
static bool isUsableSongTitle(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty())
        return false;
    if (s.length() > 120)
        return false;
    if (s.startsWith(QStringLiteral("http://"))
        || s.startsWith(QStringLiteral("https://"))
        || s.startsWith(QStringLiteral("file://"))
        || s.startsWith(QStringLiteral("about:")))
        return false;
    if (s.contains(QStringLiteral("#/")))          // e.g. "index.html#/like"
        return false;
    if (s.startsWith(QStringLiteral("index.")))
        return false;
    const QString lower = s.toLower();
    if (lower.contains(QLatin1Char('/'))
        && (lower.endsWith(QStringLiteral(".html")) || lower.endsWith(QStringLiteral(".htm"))
            || lower.endsWith(QStringLiteral(".php")) || lower.endsWith(QStringLiteral(".aspx"))))
        return false;
    return true;
}


LyricsApplet::LyricsApplet(QObject *parent)
    : DApplet(parent)
    , m_fetcher(new LyricsFetcher(this))
{
    m_posTimer.setInterval(200);
    connect(&m_posTimer, &QTimer::timeout, this, &LyricsApplet::updateLineForPosition);
    connect(m_fetcher, &LyricsFetcher::lyricsReady, this, &LyricsApplet::onLyricsReady);
    connect(m_fetcher, &LyricsFetcher::fetchFailed, this, &LyricsApplet::onLyricsFailed);
}

bool LyricsApplet::load()
{
    qWarning() << "[dock-lyrics] load()";
    return DApplet::load();
}

bool LyricsApplet::init()
{
    DApplet::init();

    QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(onNameOwnerChanged(QString, QString, QString)));

    scanPlayers();
    qWarning() << "[dock-lyrics] init done, players:" << m_players.keys() << "active:" << m_activeService;
    return true;
}

void LyricsApplet::onNameOwnerChanged(const QString &name,
                                      const QString &oldOwner,
                                      const QString &newOwner)
{
    if (!name.startsWith(kMprisPrefix))
        return;
    if (!oldOwner.isEmpty() && newOwner.isEmpty())
        removePlayer(name);
    else if (oldOwner.isEmpty() && !newOwner.isEmpty())
        addPlayer(name);
}

void LyricsApplet::scanPlayers()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("ListNames"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage)
        return;
    const QStringList names = reply.arguments().value(0).toStringList();
    for (const QString &n : names) {
        if (n.startsWith(kMprisPrefix))
            addPlayer(n);
    }
    chooseActivePlayer();
    applyActivePlayer();
}

bool LyricsApplet::addPlayer(const QString &service)
{
    qWarning() << "[dock-lyrics] addPlayer" << service;
    if (m_players.contains(service))
        return false;

    m_players.insert(service, PlayerState{});
    auto *probe = new PlayerProbe(service, this);
    m_probes.insert(service, probe);
    connect(probe, &PlayerProbe::playerPropertiesChanged,
            this, &LyricsApplet::onPlayerProperties);
    connect(probe, &PlayerProbe::playerSeeked,
            this, &LyricsApplet::onPlayerSeeked);

    readPlayerState(service);
    chooseActivePlayer();
    applyActivePlayer();
    return true;
}

void LyricsApplet::removePlayer(const QString &service)
{
    if (auto *probe = m_probes.take(service))
        probe->deleteLater();
    m_players.remove(service);
    if (m_activeService == service) {
        m_activeService.clear();
        chooseActivePlayer();
    }
    applyActivePlayer();
}

void LyricsApplet::readPlayerState(const QString &service)
{
    PlayerState &st = m_players[service];
    QDBusMessage msg = QDBusMessage::createMethodCall(
        service, kPlayerPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    msg << kPlayerIface;
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qWarning() << "[dock-lyrics] readPlayerState FAILED" << service
                   << reply.errorName() << reply.errorMessage();
        return;
    }
    const QVariantMap props = qdbusVariantToMap(reply.arguments().value(0));
    const QString status = props.value(QStringLiteral("PlaybackStatus")).toString();
    if (!status.isEmpty())
        st.status = status;
    const qulonglong pos = props.value(QStringLiteral("Position")).toULongLong();
    if (pos > 0 || props.contains(QStringLiteral("Position")))
        st.positionUs = qint64(pos);
    const QVariant meta = props.value(QStringLiteral("Metadata"));
    st.metadata = qdbusVariantToMap(meta);
    st.lastSeen = QDateTime::currentMSecsSinceEpoch();
    qWarning() << "[dock-lyrics] readPlayerState OK" << service
               << "status=" << st.status << "posUs=" << st.positionUs
               << "title=" << st.metadata.value(QStringLiteral("xesam:title")).toString();
}

void LyricsApplet::onPlayerSeeked(const QString &service, qint64 positionUs)
{
    if (service != m_activeService)
        return;
    jumpToPosition(positionUs);
    qWarning() << "[dock-lyrics] seeked to" << positionUs / 1000 << "ms"
               << "line=" << m_line;
}

void LyricsApplet::jumpToPosition(qint64 positionUs)
{
    if (m_activeService.isEmpty() || !m_players.contains(m_activeService))
        return;
    PlayerState &st = m_players[m_activeService];
    st.positionUs = positionUs;

    // Reset our internal clock to the new absolute position so the running
    // lyric timer continues correctly from here.
    m_basePositionUs = positionUs;
    m_clock.restart();
    m_positionMs = qMax<qint64>(0, positionUs / 1000);
    if (m_durationMs > 0)
        m_positionMs = qMin(m_positionMs, m_durationMs);
    emit positionChanged();

    // Recompute the displayed line immediately (works while paused too).
    if (m_synced && !m_lines.isEmpty()) {
        int idx = m_currentIndex < 0 ? 0 : m_currentIndex;
        while (idx < m_lines.size() - 1 && m_lines.at(idx + 1).timeMs <= m_positionMs)
            ++idx;
        while (idx > 0 && m_lines.at(idx).timeMs > m_positionMs)
            --idx;
        if (idx != m_currentIndex) {
            m_currentIndex = idx;
            m_line = m_lines.at(idx).text;
            emit lineChanged();
        }
    }
}

void LyricsApplet::onPlayerProperties(const QString &service, const QVariantMap &changed)
{
    if (!m_players.contains(service))
        return;
    PlayerState &st = m_players[service];
    if (changed.contains(QStringLiteral("PlaybackStatus")))
        st.status = changed.value(QStringLiteral("PlaybackStatus")).toString();
    if (changed.contains(QStringLiteral("Metadata")))
        st.metadata = changed.value(QStringLiteral("Metadata")).toMap();
    if (changed.contains(QStringLiteral("Position"))) {
        const qint64 us = changed.value(QStringLiteral("Position")).toLongLong();
        st.positionUs = us;
        if (m_activeService == service) {
            m_basePositionUs = us;
            m_clock.restart();
            m_positionMs = us / 1000;
            emit positionChanged();
        }
    }
    st.lastSeen = QDateTime::currentMSecsSinceEpoch();

    chooseActivePlayer();
    applyActivePlayer();
}

void LyricsApplet::chooseActivePlayer()
{
    // Prefer the most recently active player which is actually playing.
    QString best;
    qint64 bestSeen = -1;
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        if (it.value().status == QLatin1String("Playing") && it.value().lastSeen > bestSeen) {
            best = it.key();
            bestSeen = it.value().lastSeen;
        }
    }
    if (best.isEmpty()) {
        // Keep the current one if it merely paused, otherwise clear.
        if (!m_activeService.isEmpty() && m_players.contains(m_activeService)
            && m_players.value(m_activeService).status != QLatin1String("Stopped")) {
            return; // still active (paused)
        }
        m_activeService.clear();
        return;
    }
    if (m_activeService != best) {
        m_activeService = best;
        // Full refresh so we get complete metadata for the newly active player.
        readPlayerState(best);
    }
}

void LyricsApplet::applyActivePlayer()
{
    if (m_activeService.isEmpty() || !m_players.contains(m_activeService)) {
        qWarning() << "[dock-lyrics] no active player; playing=false";
        
        const bool wasPlaying = m_playing;
        m_playing = false;
        m_posTimer.stop();
        if (wasPlaying)
            emit playingChanged();
        return;
    }

    const PlayerState &st = m_players.value(m_activeService);
    m_playerName = m_activeService.mid(kMprisPrefix.size());

    const QString status = st.status;
    const bool nowPlaying = (status == QLatin1String("Playing"));
    const bool resumed = nowPlaying && !m_playing; // pause -> play transition
    if (nowPlaying != m_playing) {
        m_playing = nowPlaying;
        emit playingChanged();
        qWarning() << "[dock-lyrics] active" << m_playerName << "status" << status
                << "title=" << m_title << "artist=" << m_artist;
    }

    const QVariantMap meta = st.metadata;
    QString title = meta.value(QStringLiteral("xesam:title")).toString();
    if (!isUsableSongTitle(title)) {
        const QString fromUrl = QFileInfo(meta.value(QStringLiteral("xesam:url")).toString())
                                    .completeBaseName();
        title = isUsableSongTitle(fromUrl) ? fromUrl : QString();
    }
    const QString artist = toStringList(meta.value(QStringLiteral("xesam:artist"))).join(QStringLiteral(" / "));
    const QString album = meta.value(QStringLiteral("xesam:album")).toString();
    const qint64 durationUs = meta.value(QStringLiteral("mpris:length")).toLongLong();

    const bool songChangedNow = (title != m_title || artist != m_artist || album != m_album);
    m_title = title;
    m_artist = artist;
    m_album = album;
    if (durationUs > 0)
        m_durationMs = durationUs / 1000;

    if (nowPlaying) {
        m_basePositionUs = st.positionUs;
        m_clock.restart();
        m_positionMs = st.positionUs / 1000;
        if (!m_posTimer.isActive())
            m_posTimer.start();
    } else {
        m_posTimer.stop();
        m_positionMs = st.positionUs / 1000;
    }
    emit positionChanged();

    if (songChangedNow)
        emit songChanged();

    // (Re)load lyrics when the song actually changed.
    const QString key = currentSongKey();
    if (songChangedNow && m_playing && !key.isEmpty() && key != m_lyricKey) {
        qWarning() << "[dock-lyrics] song changed -> fetch lyrics for" << key;
        m_lyricKey.clear();
        requestLyricsForCurrentSong();
    } else if (resumed && m_playing && !key.isEmpty()
               && m_lyricFailed.value(key, false)) {
        // A previous lookup for this song failed (e.g. temporary network issue
        // or the online database lacked the track). Give it another chance now
        // that the user resumed playback.
        qWarning() << "[dock-lyrics] retry failed lyric lookup on resume:" << key;
        m_lyricFailed.remove(key);
        m_lyricKey.clear();
        requestLyricsForCurrentSong();
    }
}

void LyricsApplet::startPositionClock(qint64)
{
    // position clock is maintained inside applyActivePlayer()
}

void LyricsApplet::updateLineForPosition()
{
    if (!m_playing || m_lines.isEmpty() || !m_synced)
        return;

    const qint64 pos = m_basePositionUs / 1000 + m_clock.elapsed();
    m_positionMs = qBound<qint64>(0, pos, m_durationMs > 0 ? m_durationMs : pos);
    emit positionChanged();

    // binary-ish search from current index
    int idx = m_currentIndex < 0 ? 0 : m_currentIndex;
    while (idx < m_lines.size() - 1 && m_lines.at(idx + 1).timeMs <= m_positionMs)
        ++idx;
    while (idx > 0 && m_lines.at(idx).timeMs > m_positionMs)
        --idx;
    if (idx != m_currentIndex) {
        m_currentIndex = idx;
        m_line = m_lines.at(idx).text;
        emit lineChanged();
    }
}

QString LyricsApplet::currentSongKey() const
{
    const QString a = m_artist.trimmed();
    const QString t = m_title.trimmed();
    if (t.isEmpty())
        return QString();
    return (a.isEmpty() ? t : a + QStringLiteral(" - ") + t).toLower();
}

QString LyricsApplet::metadataUrlHint() const
{
    if (m_activeService.isEmpty())
        return QString();
    return m_players.value(m_activeService)
        .metadata.value(QStringLiteral("xesam:url")).toString();
}

void LyricsApplet::requestLyricsForCurrentSong()
{
    const QString key = currentSongKey();
    if (key.isEmpty() || key == m_lyricKey)
        return;
    m_lyricKey = key;

    m_lyricLines.clear();
    m_lines.clear();
    m_currentIndex = -1;
    m_line.clear();
    m_synced = false;
    m_hasLyrics = false;

    if (m_lyricCache.contains(key)) {
        const QString src = m_lyricSourceCache.value(key, QStringLiteral("local"));
        onLyricsReady(key, src, m_lyricCache.value(key));
        return;
    }
    if (m_lyricFailed.value(key, false)) {
        m_loadingLyrics = false;
        setStateText(QStringLiteral("没有找到歌词"));
        emit lyricsChanged();
        return;
    }

    m_loadingLyrics = true;
    setStateText(QStringLiteral("正在获取歌词…"));
    emit lyricsChanged();
    qWarning() << "[dock-lyrics] requestLyrics key=" << key;
    m_fetcher->requestLyrics(key, m_title, m_artist, metadataUrlHint());
}

void LyricsApplet::onLyricsReady(const QString &key, const QString &source, const QString &lrcText)
{
    if (key != m_lyricKey || key.isEmpty())
        return; // stale reply (user changed song meanwhile)

    m_loadingLyrics = false;
    m_lyricCache.insert(key, lrcText);
    m_lyricSourceCache.insert(key, source);

    QStringList plain;
    const QVector<lrc::Line> parsed = lrc::parseLrc(lrcText, &plain);
    if (!parsed.isEmpty() && parsed.first().timeMs >= 0) {
        m_lines = parsed;
        m_synced = true;
        for (const lrc::Line &ln : parsed)
            m_lyricLines << ln.text;
    } else {
        m_lines.clear();
        m_synced = false;
        m_lyricLines = plain;
        for (const QString &p : plain)
            m_lines.append({-1, p});
    }

    m_hasLyrics = !m_lyricLines.isEmpty();
    qWarning() << "[dock-lyrics] lyricsReady src=" << source << "lines=" << m_lyricLines.size();
    if (m_hasLyrics)
        setStateText(source == QLatin1String("local") ? QStringLiteral("本地歌词")
                                                      : QStringLiteral("在线歌词"));
    else
        setStateText(QStringLiteral("没有找到歌词"));

    emit lyricsChanged();
    updateLineForPosition();
}

void LyricsApplet::onLyricsFailed(const QString &key)
{
    if (key != m_lyricKey)
        return;
    m_loadingLyrics = false;
    m_lyricFailed.insert(key, true);
    m_hasLyrics = false;
    m_lyricLines.clear();
    m_lines.clear();
    setStateText(QStringLiteral("没有找到歌词"));
    emit lyricsChanged();
}

void LyricsApplet::setStateText(const QString &t)
{
    if (m_stateText == t)
        return;
    m_stateText = t;
    emit stateTextChanged();
}

void LyricsApplet::playPause()
{
    if (m_activeService.isEmpty())
        return;
    QDBusInterface player(m_activeService, kPlayerPath, kPlayerIface,
                          QDBusConnection::sessionBus());
    player.asyncCall(QStringLiteral("PlayPause"));
}

void LyricsApplet::next()
{
    if (m_activeService.isEmpty())
        return;
    QDBusInterface player(m_activeService, kPlayerPath, kPlayerIface,
                          QDBusConnection::sessionBus());
    player.asyncCall(QStringLiteral("Next"));
}

void LyricsApplet::previous()
{
    if (m_activeService.isEmpty())
        return;
    QDBusInterface player(m_activeService, kPlayerPath, kPlayerIface,
                          QDBusConnection::sessionBus());
    player.asyncCall(QStringLiteral("Previous"));
}

void LyricsApplet::refreshLyrics()
{
    const QString key = currentSongKey();
    if (key.isEmpty())
        return;
    m_lyricCache.remove(key);
    m_lyricFailed.remove(key);
    m_lyricKey.clear();
    requestLyricsForCurrentSong();
}

D_APPLET_CLASS(LyricsApplet)
#include "lyricsapplet.moc"
