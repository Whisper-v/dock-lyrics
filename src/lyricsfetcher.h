#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

// Fetch LRC lyrics for a song.
//   1. local .lrc sidecar files (next to the audio file / common music dirs)
//   2. NetEase Cloud Music (music.163.com) online database
//   3. LRCLIB (lrclib.net) open lyric database as a final fallback
// Sources are reported to the caller via the `source` argument:
//   "local" / "netease" / "lrclib".
class LyricsFetcher : public QObject
{
    Q_OBJECT
public:
    explicit LyricsFetcher(QObject *parent = nullptr);

    // key is used to ignore stale replies when the song changed quickly.
    void requestLyrics(const QString &key,
                       const QString &title,
                       const QString &artist,
                       const QString &localUrlHint);

signals:
    void lyricsReady(const QString &key, const QString &source, const QString &lrcText);
    void fetchFailed(const QString &key);

private:
    enum class Stage {
        Idle,
        NeteaseSearch,
        NeteaseLyric,
        LrclibSearch,
    };

    struct Pending {
        QString key;
        QString title;
        QString artist;
    };

    QString findLocalLrc(const QString &title, const QString &artist,
                         const QString &localUrlHint) const;

    void startNeteaseSearch();
    void startNeteaseLyric(qint64 songId);
    void startLrclibSearch();

    void doGet(const QUrl &url, Stage stage);
    void onReplyFinished();
    void emitFailure(const QString &key);

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_currentReply;
    Pending m_pending;
    Stage m_stage = Stage::Idle;
};

QString urlQueryEncode(const QString &s);
