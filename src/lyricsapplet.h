#pragma once

#include <applet.h>

#include <QElapsedTimer>
#include <QHash>
#include <QStringList>
#include <QTimer>

#include "lrcparser.h"
#include "playerprobe.h"

DS_USE_NAMESPACE

class LyricsFetcher;

class LyricsApplet : public DApplet
{
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QString playerName READ playerName NOTIFY songChanged)
    Q_PROPERTY(QString title READ title NOTIFY songChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY songChanged)
    Q_PROPERTY(QString album READ album NOTIFY songChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY songChanged)
    Q_PROPERTY(QString line READ line NOTIFY lineChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY lineChanged)
    Q_PROPERTY(QStringList lyricLines READ lyricLines NOTIFY lyricsChanged)
    Q_PROPERTY(bool synced READ synced NOTIFY lyricsChanged)
    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool loadingLyrics READ loadingLyrics NOTIFY lyricsChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY stateTextChanged)
    Q_PROPERTY(int colorTheme READ colorTheme WRITE setColorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QStringList colorThemeNames READ colorThemeNames CONSTANT)
    Q_PROPERTY(QStringList colorThemeColors READ colorThemeColors CONSTANT)
    Q_PROPERTY(QStringList colorThemeBgColors READ colorThemeBgColors CONSTANT)
    Q_PROPERTY(QString customTextColor READ customTextColor WRITE setCustomTextColor NOTIFY customColorChanged)
    Q_PROPERTY(QString customBgColor READ customBgColor WRITE setCustomBgColor NOTIFY customColorChanged)
    Q_PROPERTY(QString lyricSource READ lyricSource NOTIFY lyricsChanged)

public:
    explicit LyricsApplet(QObject *parent = nullptr);

    bool load() override;
    bool init() override;

    bool playing() const { return m_playing; }
    QString playerName() const { return m_playerName; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    qint64 positionMs() const { return m_positionMs; }
    qint64 durationMs() const { return m_durationMs; }
    QString line() const { return m_line; }
    int currentIndex() const { return m_currentIndex; }
    QStringList lyricLines() const { return m_lyricLines; }
    bool synced() const { return m_synced; }
    bool hasLyrics() const { return m_hasLyrics; }
    bool loadingLyrics() const { return m_loadingLyrics; }
    QString stateText() const { return m_stateText; }
    int colorTheme() const { return m_colorTheme; }
    void setColorTheme(int index);
    QStringList colorThemeNames() const;
    QStringList colorThemeColors() const;
    QStringList colorThemeBgColors() const;
    QString customTextColor() const { return m_customTextColor; }
    void setCustomTextColor(const QString &color);
    QString customBgColor() const { return m_customBgColor; }
    void setCustomBgColor(const QString &color);
    QString lyricSource() const { return m_lyricSource; }

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void refreshLyrics();

Q_SIGNALS:
    void playingChanged();
    void songChanged();
    void positionChanged();
    void lineChanged();
    void lyricsChanged();
    void stateTextChanged();
    void colorThemeChanged();
    void customColorChanged();

protected Q_SLOTS:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void onPlayerProperties(const QString &service, const QVariantMap &changedProperties);
    void onPlayerSeeked(const QString &service, qint64 positionUs);
    void onPlayerInvalidated(const QString &service, const QStringList &invalidatedKeys);
    struct PlayerState {
        QString status;   // Playing / Paused / Stopped
        QVariantMap metadata;
        qint64 positionUs = 0;
        qint64 lastSeen = 0;
    };

    void scanPlayers();
    bool addPlayer(const QString &service);
    void removePlayer(const QString &service);
    void readPlayerState(const QString &service);
    void chooseActivePlayer();
    void applyActivePlayer();
    void startPositionClock(qint64 positionUs);
    void requestLyricsForCurrentSong();
    void onLyricsReady(const QString &key, const QString &source, const QString &lrcText);
    void onLyricsFailed(const QString &key);
    void updateLineForPosition();
    void jumpToPosition(qint64 positionUs);
    void setStateText(const QString &t);

    QString currentSongKey() const;
    QString metadataUrlHint() const;

private:
    QString m_activeService;
    QHash<QString, PlayerState> m_players;
    QHash<QString, PlayerProbe *> m_probes;
    QString m_playerName;
    bool m_playing = false;
    QString m_title;
    QString m_artist;
    QString m_album;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;

    QVector<lrc::Line> m_lines;        // timed lines (synced mode)
    QStringList m_lyricLines;          // display-only lines
    bool m_synced = false;
    bool m_hasLyrics = false;
    bool m_loadingLyrics = false;
    int m_currentIndex = -1;
    QString m_line;
    QString m_stateText;
    QString m_lyricSource;
    int m_colorTheme = 0;
    QString m_customTextColor;
    QString m_customBgColor;

    QString m_lyricKey;                // song key the current lyrics belong to
    QHash<QString, QString> m_lyricCache;
    QHash<QString, QString> m_lyricSourceCache;
    QHash<QString, bool> m_lyricFailed;

    QElapsedTimer m_clock;
    qint64 m_basePositionUs = 0;
    QTimer m_posTimer;
    LyricsFetcher *m_fetcher = nullptr;
};
