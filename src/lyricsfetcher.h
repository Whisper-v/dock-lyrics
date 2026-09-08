#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

// Fetch LRC lyrics for a song. Local .lrc files are searched first,
// then the online NetEase Cloud Music lyrics database is queried.
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
    struct Pending {
        QString key;
        QString title;
        QString artist;
    };

    QString findLocalLrc(const QString &title, const QString &artist,
                         const QString &localUrlHint) const;
    void startOnlineSearch(const Pending &p);
    void onSearchFinished();
    void startLyricRequest(qint64 songId);
    void onLyricFinished();

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_currentReply;
    Pending m_pending;
};

QString urlQueryEncode(const QString &s);
