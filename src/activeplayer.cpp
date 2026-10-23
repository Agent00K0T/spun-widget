#include "activeplayer.h"
#include "palette.h"
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QBuffer>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtConcurrent>
#include <QUrlQuery>
#include <utility>

static const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
static const QString iface = QStringLiteral("org.mpris.MediaPlayer2.Player");
static const QString props = QStringLiteral("org.freedesktop.DBus.Properties");
static QVariant unwrap(QVariant v) { return v.metaType() == QMetaType::fromType<QDBusVariant>() ? v.value<QDBusVariant>().variant() : v; }
static QVariantMap asMap(QVariant v) {
    v = unwrap(v);
    return v.metaType() == QMetaType::fromType<QDBusArgument>() ? qdbus_cast<QVariantMap>(v.value<QDBusArgument>()) : v.toMap();
}

// The cache key of a page: one key per YouTube video whatever the playlist or link form, the URL otherwise
static QString videoKey(const QString &pageUrl) {
    const QUrl url(pageUrl);
    const QString host = url.host();
    if (host == "youtu.be") return "youtube:" + url.path().section('/', 1, 1);
    if (host != "youtube.com" && !host.endsWith(".youtube.com")) return pageUrl;
    const auto path = url.path().split('/', Qt::SkipEmptyParts);
    if (path.size() >= 2 && (path[0] == "shorts" || path[0] == "live" || path[0] == "embed")) return "youtube:" + path[1];
    const auto id = QUrlQuery(url).queryItemValue("v");
    return id.isEmpty() ? pageUrl : "youtube:" + id;
}

// YouTube plays muted previews of the videos under the pointer on its home, search and channel
// pages; Firefox reports them as playback of that page. Only a page of one video is a track.
static bool youtubePreview(const QString &pageUrl) {
    const QString host = QUrl(pageUrl).host();
    const bool youtube = host == "youtu.be" || host == "youtube.com" || host.endsWith(".youtube.com");
    return youtube && videoKey(pageUrl) == pageUrl;
}
static bool youtubePreview(const QVariantMap &metadata) { return youtubePreview(unwrap(metadata.value("xesam:url")).toString()); }

ActivePlayer::ActivePlayer(QObject *parent) : QObject(parent) {
    m_commandClock.start();
    m_commands.setInterval(50);
    connect(&m_commands, &QTimer::timeout, this, &ActivePlayer::flushCommands);
    m_tick.setInterval(200);
    connect(&m_tick, &QTimer::timeout, this, [this] { if (m_playing) { emit positionChanged(); prefetchNext(); } });
    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, [this] { if (!m_service.isEmpty()) refresh(); });
    m_poll.start();
    m_playerScan.setInterval(1000);
    connect(&m_playerScan, &QTimer::timeout, this, &ActivePlayer::scanPlayers);
    m_playerScan.start();
    scanPlayers();
}
ActivePlayer::~ActivePlayer() { if (m_artDecodeActive) m_artLoader.waitForFinished(); }

void ActivePlayer::setChaptersEnabled(bool enabled) {
    if (enabled == m_chaptersEnabled) return;
    m_chaptersEnabled = enabled;
    emit chaptersEnabledChanged();
    if (!m_pageUrl.isEmpty()) fetchChapters();
}
void ActivePlayer::setPlayerFilter(const QString &filter) {
    if (filter.trimmed() == m_playerFilter) return;
    m_playerFilter = filter.trimmed();
    emit playerFilterChanged();
    scanPlayers();
}
// Like Noctalia's media widget: prefer a playing player (keeping the current one if it
// still plays), otherwise stay on the current player, otherwise take the first one.
void ActivePlayer::scanPlayers() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.interface()) return;
    QStringList players;
    for (const auto &name : bus.interface()->registeredServiceNames().value())
        if (name.startsWith("org.mpris.MediaPlayer2.") && name != "org.mpris.MediaPlayer2.playerctld"
            && (m_playerFilter.isEmpty() || name.mid(23).contains(m_playerFilter, Qt::CaseInsensitive)))
            players.append(name);
    players.sort();
    QString playing;
    for (const auto &name : players) {
        auto message = QDBusMessage::createMethodCall(name, path, props, "Get");
        message << iface << QStringLiteral("PlaybackStatus");
        const auto reply = bus.call(message, QDBus::Block, 300);
        if (reply.type() != QDBusMessage::ReplyMessage || unwrap(reply.arguments().value(0)).toString() != "Playing") continue;
        // A playing preview doesn't take the widget over from another player
        auto metadata = QDBusMessage::createMethodCall(name, path, props, "Get");
        metadata << iface << QStringLiteral("Metadata");
        if (const auto answer = bus.call(metadata, QDBus::Block, 300); answer.type() == QDBusMessage::ReplyMessage && youtubePreview(asMap(answer.arguments().value(0)))) continue;
        if (name == m_service || playing.isEmpty()) playing = name;
    }
    const QString next = !playing.isEmpty() ? playing : players.contains(m_service) ? m_service : players.value(0);
    if (next != m_service) bindPlayer(next);
}
void ActivePlayer::bindPlayer(const QString &name) {
    auto bus = QDBusConnection::sessionBus();
    if (!m_service.isEmpty()) {
        bus.disconnect(m_service, path, props, "PropertiesChanged", this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        bus.disconnect(m_service, path, iface, "Seeked", this, SLOT(seeked(qlonglong)));
    }
    m_service = name;
    m_refreshing = false;
    clearPlayback();
    if (m_service.isEmpty()) return;
    bus.connect(m_service, path, props, "PropertiesChanged", this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(m_service, path, iface, "Seeked", this, SLOT(seeked(qlonglong)));
    refresh();
}
void ActivePlayer::clearPlayback() {
    m_tick.stop();
    m_preview = false;
    m_pageUrl.clear(); m_pageKey.clear();
    if (!m_chapters.isEmpty()) { m_chapters.clear(); emit chaptersChanged(); }
    m_available = m_playing = false; m_title.clear(); m_artist.clear(); m_album.clear();
    m_position = m_duration = 0; m_track.clear(); m_artUrl = QUrl(); loadArt({});
    emit trackChanged(); emit playingChanged(); emit positionChanged();
}
qint64 ActivePlayer::position() const {
    return qBound<qint64>(0, m_position + (m_playing && m_clock.isValid() ? m_clock.elapsed() : 0), m_duration);
}
void ActivePlayer::refresh() {
    if (m_refreshing) return;
    m_refreshing = true;
    auto message = QDBusMessage::createMethodCall(m_service, path, props, "GetAll");
    message << iface;
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    const auto metadataRevision = m_metadataRevision;
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch, metadataRevision] {
        QDBusPendingReply<QVariantMap> reply = *watch;
        watch->deleteLater(); m_refreshing = false;
        if (reply.isError()) {
            if (m_available) { m_tick.stop(); m_available = m_playing = false; emit trackChanged(); emit playingChanged(); }
            return;
        }
        const bool wasAvailable = m_available; m_available = true;
        auto values = reply.value();
        // A PropertiesChanged signal can overtake an older GetAll reply.
        if (metadataRevision != m_metadataRevision) { values.remove("Metadata"); values.remove("Position"); }
        apply(values);
        if (!wasAvailable) emit trackChanged();
    });
}
void ActivePlayer::apply(const QVariantMap &v) {
    // While a YouTube preview plays, the player's reports are about it: keep showing the last
    // real track (as it was) until a video page plays again
    if (v.contains("Metadata")) m_preview = youtubePreview(asMap(v.value("Metadata")));
    if (m_preview) return;
    const auto previousPosition = position();
    bool metadataChanged = false;
    if (v.contains("Metadata")) {
        const auto meta = asMap(v.value("Metadata"));
        const auto title = unwrap(meta.value("xesam:title")).toString();
        const auto artist = unwrap(meta.value("xesam:artist")).toStringList().join(", ");
        const auto album = unwrap(meta.value("xesam:album")).toString();
        auto id = unwrap(meta.value("mpris:trackid"));
        const auto track = id.canConvert<QDBusObjectPath>() ? id.value<QDBusObjectPath>().path() : id.toString();
        auto duration = unwrap(meta.value("mpris:length")).toLongLong()/1000;
        const bool identityChanged = title != m_title || artist != m_artist || album != m_album || track != m_track;
        const auto pageUrl = unwrap(meta.value("xesam:url")).toString();
        // Browsers drop mpris:length while paused or seeking; keep the same track's known length
        if (duration <= 0 && !identityChanged && pageUrl == m_pageUrl) duration = m_duration;
        metadataChanged = identityChanged || duration != m_duration;
        const auto previousDuration = m_duration;
        if (track != m_track) { m_position = 0; m_clock.restart(); }
        m_title = title; m_artist = artist; m_album = album; m_track = track; m_duration = duration;
        if (pageUrl != m_pageUrl) {
            m_previousRings = {{"chapters", m_chapters}, {"duration", previousDuration}, {"position", previousPosition},
                               {"outer", m_ringOuter}, {"inner", m_ringInner}};
            m_pageUrl = pageUrl;
            fetchChapters();
        }
        // A browser reports a new video's length only a second after switching to it; remember it,
        // so returning to the video shows its chapter rings with the record already
        if (m_duration > 0 && !m_pageKey.isEmpty()) m_durationCache.insert(m_pageKey, m_duration);
        const auto url = QUrl(unwrap(meta.value("mpris:artUrl")).toString());
        if (identityChanged || url != m_artUrl) { m_artUrl = url; loadArt(url); }
    }
    if (v.contains("PlaybackStatus")) {
        const auto status = unwrap(v.value("PlaybackStatus")).toString();
        const bool isPlaying = status == "Playing";
        const bool statusChanged = m_playbackStatus != status;
        m_playbackStatus = status;
        if (isPlaying != m_playing || statusChanged) {
            m_position = metadataChanged ? 0 : previousPosition;
            m_playing = isPlaying; m_clock.restart();
            if (m_playing) m_tick.start(); else m_tick.stop();
            emit playingChanged();
        }
    }
    // Browsers also report position 0 while paused or seeking; ignore it mid-track
    const bool positionGlitch = !metadataChanged && m_duration > 0 && previousPosition > 1500
        && v.contains("Position") && unwrap(v.value("Position")).toLongLong() == 0;
    if (v.contains("Position") && !positionGlitch) {
        m_position = unwrap(v.value("Position")).toLongLong()/1000; m_clock.restart();
        if (position() != previousPosition) emit positionChanged();
    }
    if (v.contains("Volume")) {
        const auto volume = unwrap(v.value("Volume")).toDouble();
        if (volume != m_volume) { m_volume = volume; emit volumeChanged(); }
    }
    const bool oldSeek = m_canSeek, oldNext = m_canNext;
    if (v.contains("CanSeek")) m_canSeek = unwrap(v.value("CanSeek")).toBool();
    if (v.contains("CanGoNext")) m_canNext = unwrap(v.value("CanGoNext")).toBool();
    if (metadataChanged || oldSeek != m_canSeek || oldNext != m_canNext) emit trackChanged();
}
void ActivePlayer::propertiesChanged(const QString &interface, const QVariantMap &values, const QStringList &invalidated) {
    if (interface != iface) return;
    if (values.contains("Metadata") || invalidated.contains("Metadata")) ++m_metadataRevision;
    apply(values);
    if (!invalidated.isEmpty()) refresh();
}
void ActivePlayer::seeked(qlonglong value) {
    if (m_preview) return;
    m_position = value/1000; m_clock.restart(); emit positionChanged();
}
void ActivePlayer::call(const QString &method, const QVariantList &args) {
    if (!m_available) return;
    auto message = QDBusMessage::createMethodCall(m_service, path, iface, method); message.setArguments(args);
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch] {
        watch->deleteLater(); QTimer::singleShot(160, this, &ActivePlayer::refresh);
    });
}
void ActivePlayer::seek(qint64 ms) {
    if (!m_canSeek || !m_track.startsWith('/')) return;
    const auto position = qBound<qint64>(0, ms, m_duration);
    const auto track = m_track;
    schedule("seek", [this, track, position] {
        if (track != m_track) return;
        // Show the new position right away; the player confirms it on the next poll
        m_position = position; m_clock.restart(); emit positionChanged();
        call("SetPosition", {QVariant::fromValue(QDBusObjectPath(track)), QVariant::fromValue(position*1000)});
    });
}
void ActivePlayer::setVolume(double value) {
    value = qBound(0.0, value, 1.0);
    if (!m_available) return;
    schedule("volume", [this, value] {
        auto message = QDBusMessage::createMethodCall(m_service, path, props, "Set");
        message << iface << QStringLiteral("Volume") << QVariant::fromValue(QDBusVariant(value));
        auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
        connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch] { watch->deleteLater(); refresh(); });
    });
}
void ActivePlayer::schedule(const QString &key, std::function<void()> action) {
    m_pendingCommands[key] = std::move(action);
    flushCommands();
    if (!m_pendingCommands.isEmpty()) m_commands.start();
}
void ActivePlayer::flushCommands() {
    const auto keys = m_pendingCommands.keys();
    for (const auto &key : keys) {
        if (m_commandClock.elapsed() - m_lastCommand.value(key, -1000) < 750) continue;
        auto action = m_pendingCommands.take(key);
        m_lastCommand[key] = m_commandClock.elapsed();
        action();
    }
    if (m_pendingCommands.isEmpty()) m_commands.stop();
}

// The chapter helper: installed under libexec, or next to the binary in a build tree
static QString chapterHelper() {
    const QDir dir(QCoreApplication::applicationDirPath());
    const QString installed = dir.filePath(QStringLiteral(SPUN_WIDGET_HELPER_DIR "/yt-chapters"));
    return QFileInfo::exists(installed) ? installed : dir.filePath("yt-chapters");
}

// Chapters come from YouTube (scripts/yt-chapters, one small request per video, cached for the session).
void ActivePlayer::fetchChapters() {
    m_pageKey = videoKey(m_pageUrl);
    const QString key = m_pageKey;
    // Testing aid: SPUN_TEST_CHAPTERS='{"duration":ms,"chapters":[{"start":ms,"title":"…"},…]}'
    // replaces the chapters of whatever plays (any player)
    if (const auto test = qgetenv("SPUN_TEST_CHAPTERS"); !test.isEmpty()) {
        const auto json = QJsonDocument::fromJson(test).object();
        if (json.value("duration").toDouble() > 0) m_durationCache.insert(key, qint64(json.value("duration").toDouble()));
        m_chapterCache.insert(key, json.value("chapters").toArray().toVariantList());
        m_chapters = m_chapterCache.value(key); emit chaptersChanged(); applyPageDuration();
        return;
    }
    // With chapters switched off YouTube is never asked (and nothing is prefetched)
    const bool youtube = m_chaptersEnabled && key.startsWith("youtube:");
    if (!youtube || m_chapterCache.contains(key)) {
        m_chapters = youtube ? m_chapterCache.value(key) : QVariantList();
        emit chaptersChanged();
        applyPageDuration();
        return;
    }
    if (!m_chapters.isEmpty()) { m_chapters.clear(); emit chaptersChanged(); }
    requestChapters(key, m_pageUrl);
}
// Runs scripts/yt-chapters for a video; the result is applied if that video is still playing.
// A failure (no network, YouTube's bot check) is not cached: the playing video is retried in 10 minutes.
void ActivePlayer::requestChapters(const QString &key, const QString &url) {
    if (m_chapterRequests.contains(key)) return;
    m_chapterRequests.insert(key);
    auto *job = new QProcess(this);
    connect(job, &QProcess::finished, this, [this, job, key, url](int code, QProcess::ExitStatus status) {
        job->deleteLater();
        m_chapterRequests.remove(key);
        if (status != QProcess::NormalExit || code != 0) {
            QTimer::singleShot(10 * 60 * 1000, this, [this, key, url] { if (key == m_pageKey && !m_chapterCache.contains(key)) requestChapters(key, url); });
            return;
        }
        // Lines: duration in seconds (or NA), chapters JSON, the ID of the video that comes next
        const auto lines = QString::fromUtf8(job->readAllStandardOutput()).split('\n');
        if (const double seconds = lines.value(0).toDouble(); seconds > 0) m_durationCache.insert(key, qint64(seconds * 1000));
        QVariantList result;
        for (const auto &value : QJsonDocument::fromJson(lines.value(1).toUtf8()).array()) {
            const auto chapter = value.toObject();
            result.append(QVariantMap{{"start", qint64(chapter.value("start_time").toDouble() * 1000)},
                                      {"title", chapter.value("title").toString()}});
        }
        m_chapterCache.insert(key, result);
        if (const auto next = lines.value(2).trimmed(); !next.isEmpty()) m_nextVideo.insert(key, next);
        if (key == m_pageKey) { m_chapters = result; emit chaptersChanged(); applyPageDuration(); }
    });
    connect(job, &QProcess::errorOccurred, job, [this, job, key] { m_chapterRequests.remove(key); job->deleteLater(); });
    // The playing video's own URL keeps its playlist, so "next" is the playlist's next video
    job->start(chapterHelper(), {url});
}
// Near the end of a YouTube video (last 30 s) the chapters of the one that comes next are fetched,
// so its record arrives with its rings. Only one video ahead, once.
void ActivePlayer::prefetchNext() {
    if (!m_chaptersEnabled || m_duration <= 0 || position() < m_duration - 30000) return;
    const auto next = m_nextVideo.value(m_pageKey);
    if (next.isEmpty()) return;
    const QString key = "youtube:" + next;
    if (m_chapterCache.contains(key) || m_prefetched.contains(key)) return;
    m_prefetched.insert(key);
    requestChapters(key, "https://www.youtube.com/watch?v=" + next);
}
// Browsers report no length while paused or right after switching videos; use the one known before.
void ActivePlayer::applyPageDuration() {
    const qint64 duration = m_durationCache.value(m_pageKey);
    if (duration <= 0 || m_duration > 0) return;
    m_duration = duration;
    emit trackChanged(); emit positionChanged();
}

void ActivePlayer::loadArt(const QUrl &url) {
    ++m_artGeneration; m_pendingArtBytes.clear(); m_pendingArtFile.clear();
    if (m_artReply) { auto *previous = m_artReply.data(); m_artReply = nullptr; previous->abort(); previous->deleteLater(); }
    m_art = {}; emit artworkChanged();
    // Until the new cover is decoded the rings keep the previous colours; without a cover they are grey
    if (url.isEmpty()) setRingColors({});
    if (url.isLocalFile()) { m_pendingArtFile = url.toLocalFile(); decodeArt(); return; }
    if (url.scheme() != "https" && url.scheme() != "http") return;
    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    m_artReply = m_network.get(request);
    auto *reply = m_artReply.data();
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64 total) { if (received > 12*1024*1024 || total > 12*1024*1024) reply->abort(); });
    const auto generation = m_artGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        if (m_artReply == reply) m_artReply = nullptr;
        if (generation == m_artGeneration && reply->error() == QNetworkReply::NoError) {
            m_pendingArtBytes = reply->readAll();
            decodeArt();
        }
        reply->deleteLater();
    });
}
// Covers are decoded off the GUI thread, at most 1200 px.
void ActivePlayer::decodeArt() {
    if (m_artDecodeActive || (m_pendingArtBytes.isEmpty() && m_pendingArtFile.isEmpty())) return;
    m_artDecodeActive = true;
    const auto generation = m_artGeneration;
    auto bytes = std::exchange(m_pendingArtBytes, {});
    auto file = std::exchange(m_pendingArtFile, {});
    m_artLoader.disconnect(this);
    connect(&m_artLoader, &QFutureWatcherBase::finished, this, [this, generation] {
        m_artDecodeActive = false;
        if (generation == m_artGeneration) {
            m_art = m_artLoader.result();
            emit artworkChanged();
            setRingColors(m_art);
        }
        decodeArt();
    });
    m_artLoader.setFuture(QtConcurrent::run([bytes=std::move(bytes), file=std::move(file)] {
        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader;
        if (!file.isEmpty()) reader.setFileName(file);
        else reader.setDevice(&buffer);
        const auto original = reader.size();
        const QSize limit(1200,1200);
        if (original.isValid() && (file.isEmpty() || original.width()>1200 || original.height()>1200))
            reader.setScaledSize(original.scaled(limit,Qt::KeepAspectRatio));
        QImage art = reader.read();
        if (file.isEmpty() && !art.isNull())
            art = art.scaled(limit,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        return art;
    }));
}

// Two colours of the cover for the chapter rings (see coverColors); a cover with one colour gets
// a darker shade of it. Lightness is pulled into a range that stays visible on the record.
// A grey cover gives grey rings.
void ActivePlayer::setRingColors(const QImage &art) {
    QColor outer("#b4b4b4"), inner("#6e6e6e");
    if (const auto colours = coverColors(art, 2); !colours.isEmpty()) {
        QColor a = colours[0], b = colours.size() > 1 ? colours[1] : colours[0].darker(160);
        if (a.lightnessF() < b.lightnessF()) std::swap(a, b);
        const auto withLightness = [](QColor c, double low, double high) {
            return QColor::fromHslF(qMax(0.f, c.hslHueF()), c.hslSaturationF(), qBound(low, double(c.lightnessF()), high));
        };
        outer = withLightness(a, .58, .85);
        inner = withLightness(b, .36, .62);
    }
    if (outer == m_ringOuter && inner == m_ringInner) return;
    m_ringOuter = outer; m_ringInner = inner;
    emit ringColorsChanged();
}
