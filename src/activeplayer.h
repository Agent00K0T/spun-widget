#pragma once
#include <QObject>
#include <QImage>
#include <QColor>
#include <QTimer>
#include <QDBusMessage>
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
    Q_PROPERTY(QString albumId READ albumId NOTIFY trackChanged)
    Q_PROPERTY(QString songKey READ songKey NOTIFY trackChanged)
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
    // Widget settings: YouTube chapters on/off; players by parts of their MPRIS names: only these
    // (empty: any), never those (they win over onlyPlayers)
    Q_PROPERTY(bool chaptersEnabled READ chaptersEnabled WRITE setChaptersEnabled NOTIFY chaptersEnabledChanged)
    Q_PROPERTY(QStringList onlyPlayers READ onlyPlayers WRITE setOnlyPlayers NOTIFY playerFilterChanged)
    Q_PROPERTY(QStringList ignoredPlayers READ ignoredPlayers WRITE setIgnoredPlayers NOTIFY playerFilterChanged)
public:
    explicit ActivePlayer(QObject *parent = nullptr);
    ~ActivePlayer() override;
    // A track is shown only when the player names it
    int count() const { return m_available && !m_title.isEmpty() ? 1 : 0; }
    // Browsers use one constant track id for every track, so the track is also told by its name
    QString trackKey() const { return count() ? m_track+"\n"+m_title+"\n"+m_artist+"\n"+m_pageUrl : QString{}; }
    // The record: the album; without one a YouTube video is a record (its ads change the title),
    // elsewhere every track is (music sites keep the page while tracks change)
    QString albumKey() const {
        const auto album = albumId();
        if (!album.isEmpty()) return album;
        if (m_pageKey.startsWith("youtube:")) return m_pageKey;
        return m_pageKey+"\n"+m_title+"\n"+m_artist;
    }
    // The album the record holds (the record turns over between its tracks): a YouTube album
    // playlist (list=OLAK5uy_…; YouTube Music sends the album only for some tracks), or the
    // player's album; empty when unknown
    QString albumId() const;
    // A track of that album; on YouTube the video (an ad before it changes the title, not the video)
    QString songKey() const { return count() && m_pageKey.startsWith("youtube:") ? m_pageKey : trackKey(); }
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
    QStringList onlyPlayers() const { return m_onlyPlayers; }
    void setOnlyPlayers(const QStringList &players);
    QStringList ignoredPlayers() const { return m_ignoredPlayers; }
    void setIgnoredPlayers(const QStringList &players);
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
    // Any MPRIS player's PropertiesChanged and the bus's NameOwnerChanged: rescan the players
    void anyPlayerChanged(const QDBusMessage &message);
    void nameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
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

    QString m_service;
    QString m_serviceOwner;   // its unique bus name: the sender of its signals
    QStringList m_onlyPlayers, m_ignoredPlayers;
    bool playerAllowed(const QString &service) const;
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
    QTimer m_tick;
    // One rescan for a burst of player events (a new player reports all its state at once)
    QTimer m_rescan;
    // Repeated seeks and volume changes go out at most every 750 ms per kind
    QTimer m_commands;
    QElapsedTimer m_commandClock;
    QMap<QString, qint64> m_lastCommand;
    QMap<QString, std::function<void()>> m_pendingCommands;
    QVariantList m_chapters;
    QVariantMap m_previousRings;
    // Keyed by video ("youtube:<id>"), see videoKey()
    QString m_pageKey;
    // Browsers save every track's cover to a new numbered file (Firefox: firefox-mpris/<pid>_<n>.png,
    // Chromium: /tmp/.org.chromium.Chromium.<random>) and report it a moment after the track, so
    // for a site the site stands for the cover
    QString artKey() const {
        const QUrl page(m_pageUrl);
        const auto path = m_artUrl.path();
        const bool web = page.scheme() == "http" || page.scheme() == "https";
        return web || path.contains("/firefox-mpris/") || path.contains("/.org.chromium.") ? page.host() : path;
    }
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
    // The album of the cover in m_art
    QString m_artAlbum;
    quint64 m_artGeneration = 0;
    bool m_artDecodeActive = false;
};
