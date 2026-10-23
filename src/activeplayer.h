#pragma once
#include <QObject>
#include <QImage>
#include <QColor>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QFutureWatcher>
#include <QHash>
#include <QSet>
#include <QMap>
#include <QUrl>
#include <QVariantList>
#include <functional>

// Follows whichever MPRIS player is active (prefers one that plays) and controls it.
// The MPRIS part of Spun's Cider client (src/cider.h, https://github.com/yappologistic/Spun)
// plus YouTube chapters for the rings and needle snapping.
class ActivePlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY trackChanged)
    Q_PROPERTY(QString trackKey READ trackKey NOTIFY trackChanged)
    Q_PROPERTY(QString albumKey READ albumKey NOTIFY trackChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY trackChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY trackChanged)
    // YouTube chapters of the current video, [{start, title}] in ms
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    // The previous video's chapters, length and position (ms), for the record that is swapped out
    Q_PROPERTY(QVariantMap previousRings READ previousRings NOTIFY chaptersChanged)
    Q_PROPERTY(QImage artwork READ artwork NOTIFY artworkChanged)
    // Two colours of the cover for the chapter rings: the lighter one for the rim side
    Q_PROPERTY(QColor ringOuter READ ringOuter NOTIFY ringColorsChanged)
    Q_PROPERTY(QColor ringInner READ ringInner NOTIFY ringColorsChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    // Widget settings: YouTube chapters on/off, and only players whose name contains the filter
    Q_PROPERTY(bool chaptersEnabled READ chaptersEnabled WRITE setChaptersEnabled NOTIFY chaptersEnabledChanged)
    Q_PROPERTY(QString playerFilter READ playerFilter WRITE setPlayerFilter NOTIFY playerFilterChanged)
public:
    explicit ActivePlayer(QObject *parent = nullptr);
    ~ActivePlayer() override;
    // A track is shown only when the player names it
    int count() const { return m_available && !m_title.isEmpty() ? 1 : 0; }
    QString trackKey() const { return count() ? m_track : QString{}; }
    // Browsers use one constant track id for every video, so a new video is a new record
    QString albumKey() const { return m_album.isEmpty() ? (m_pageKey.isEmpty() ? m_title : m_pageKey) : m_album+"|"+m_artUrl.path(); }
    qint64 duration() const { return m_duration; }
    bool canSeek() const { return m_canSeek; }
    bool canNext() const { return m_canNext; }
    QVariantList chapters() const { return m_chapters; }
    QVariantMap previousRings() const { return m_previousRings; }
    QImage artwork() const { return m_art; }
    QColor ringOuter() const { return m_ringOuter; }
    QColor ringInner() const { return m_ringInner; }
    bool playing() const { return m_playing; }
    qint64 position() const;
    double volume() const { return m_volume; }
    void setVolume(double value);
    bool chaptersEnabled() const { return m_chaptersEnabled; }
    void setChaptersEnabled(bool enabled);
    QString playerFilter() const { return m_playerFilter; }
    void setPlayerFilter(const QString &filter);
    Q_INVOKABLE void play() { call("Play"); }
    Q_INVOKABLE void pause() { call("Pause"); }
    Q_INVOKABLE void next() { if (m_canNext) call("Next"); }
    Q_INVOKABLE void seek(qint64 milliseconds);
    // The position right now (the property only notifies every 200 ms)
    Q_INVOKABLE qint64 currentPosition() const { return position(); }
signals:
    void trackChanged();
    void chaptersChanged();
    void artworkChanged();
    void ringColorsChanged();
    void playingChanged();
    void positionChanged();
    void volumeChanged();
    void chaptersEnabledChanged();
    void playerFilterChanged();
private slots:
    void propertiesChanged(const QString &interface, const QVariantMap &values, const QStringList &invalidated);
    void seeked(qlonglong microseconds);
private:
    void scanPlayers();
    void bindPlayer(const QString &name);
    void clearPlayback();
    void refresh();
    void apply(const QVariantMap &values);
    void call(const QString &method, const QVariantList &args = {});
    void schedule(const QString &key, std::function<void()> action);
    void flushCommands();
    void fetchChapters();
    void requestChapters(const QString &key, const QString &url);
    void prefetchNext();
    void applyPageDuration();
    void loadArt(const QUrl &url);
    void decodeArt();

    QString m_service, m_playerFilter;
    bool m_chaptersEnabled = true;
    bool m_preview = false;   // the player is playing a YouTube hover preview
    bool m_available = false, m_playing = false, m_refreshing = false;
    bool m_canSeek = false, m_canNext = false;
    QString m_playbackStatus = "Stopped";
    QString m_title, m_artist, m_album, m_track, m_pageUrl;
    qint64 m_position = 0, m_duration = 0;
    double m_volume = 1;
    quint64 m_metadataRevision = 0;
    QElapsedTimer m_clock;
    QTimer m_tick, m_poll, m_playerScan;
    // Repeated seeks and volume changes go out at most every 750 ms per kind
    QTimer m_commands;
    QElapsedTimer m_commandClock;
    QMap<QString, qint64> m_lastCommand;
    QMap<QString, std::function<void()>> m_pendingCommands;
    QVariantList m_chapters;
    QVariantMap m_previousRings;
    // Keyed by video ("youtube:<id>"), see videoKey()
    QString m_pageKey;
    QHash<QString, QVariantList> m_chapterCache;
    QHash<QString, qint64> m_durationCache;   // ms, reported earlier or from the video page
    QHash<QString, QString> m_nextVideo;      // the video YouTube plays after this one
    QSet<QString> m_chapterRequests, m_prefetched;
    QUrl m_artUrl;
    QImage m_art;
    QColor m_ringOuter = QColor("#b4b4b4"), m_ringInner = QColor("#6e6e6e");
    void setRingColors(const QImage &art);
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_artReply;
    QFutureWatcher<QImage> m_artLoader;
    QByteArray m_pendingArtBytes;
    QString m_pendingArtFile;
    quint64 m_artGeneration = 0;
    bool m_artDecodeActive = false;
};
