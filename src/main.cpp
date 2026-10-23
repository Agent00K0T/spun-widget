// Spun Widget: the record player of Spun (https://github.com/yappologistic/Spun, commit d4448ec)
// as a desktop widget for Hyprland + Noctalia, showing and controlling the active MPRIS player.
#include "activeplayer.h"
#include "disc.h"
#include "playerbody.h"
#include "theme.h"
#include <LayerShellQt/Window>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegion>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QTimer>
#include <QTranslator>
#include <QLocale>
#include <optional>

// Whether the window is on screen. Animations pause while it is covered (no frames are drawn then).
class WindowState : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exposed READ exposed NOTIFY exposureChanged)
public:
    void attach(QWindow *window) {
        window->installEventFilter(this);
        if (window->isExposed() != m_exposed) { m_exposed = window->isExposed(); emit exposureChanged(); }
    }
    bool exposed() const { return m_exposed; }
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Expose)
            if (auto *window = qobject_cast<QWindow *>(object); window && window->isExposed() != m_exposed) {
                m_exposed = window->isExposed(); emit exposureChanged();
            }
        return QObject::eventFilter(object, event);
    }
signals:
    void exposureChanged();
private:
    bool m_exposed = false;
};

// Geometry and settings of the "<author>/spun-widget:spun" desktop widget in Noctalia's settings: the
// user moves and resizes that (empty) frame in Noctalia's widget editor and the widget follows it.
// Its settings (declared in the plugin's plugin.toml) are in the frame's ".settings" table.
struct NoctaliaFrame { QString output; double cx = 0, cy = 0, width = 0, height = 0, placementWidth = 0, placementHeight = 0; QVariantMap settings; };
static QString noctaliaSettingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/noctalia/settings.toml";
}
// Noctalia saves widget settings to settings.toml only when its settings menu closes; the
// plugin's script (anchor.luau) publishes them here at once, and these values win.
// The plugin's data directory is named after its id (<author>/spun-widget); take the newest file.
static QString publishedSettingsPath() {
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/noctalia/plugins/data");
    QString newest; QDateTime newestTime;
    for (const auto &author : data.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QFileInfo file(data.filePath(author + "/spun-widget/settings.json"));
        if (file.exists() && (newest.isEmpty() || file.lastModified() > newestTime)) { newest = file.filePath(); newestTime = file.lastModified(); }
    }
    return newest;
}
static QVariantMap publishedSettings() {
    const QString path = publishedSettingsPath();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}
static std::optional<NoctaliaFrame> noctaliaFrame() {
    QFile file(noctaliaSettingsPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return std::nullopt;
    const QRegularExpression header(QStringLiteral("^\\s*\\[desktop_widgets\\.widget\\.([^\\].]+)(\\.settings)?\\]\\s*$"));
    const QRegularExpression keyValue(QStringLiteral("^\\s*([a-z_0-9]+)\\s*=\\s*(\"?)([^\"]*)\"?\\s*$"));
    // Per widget id: its keys, and its settings (strings, booleans and numbers)
    QHash<QString, QHash<QString, QString>> widgets;
    QHash<QString, QVariantMap> settings;
    QString id; bool inSettings = false;
    for (const auto &line : QString::fromUtf8(file.readAll()).split('\n')) {
        if (const auto match = header.match(line); match.hasMatch()) { id = match.captured(1); inSettings = !match.captured(2).isEmpty(); continue; }
        if (line.trimmed().startsWith('[')) { id.clear(); continue; }
        if (id.isEmpty()) continue;
        const auto match = keyValue.match(line);
        if (!match.hasMatch()) continue;
        const QString key = match.captured(1), value = match.captured(3);
        if (!inSettings) { widgets[id].insert(key, value); continue; }
        if (!match.captured(2).isEmpty()) settings[id].insert(key, value);
        else if (value == "true" || value == "false") settings[id].insert(key, value == "true");
        else settings[id].insert(key, value.toDouble());
    }
    for (auto it = widgets.cbegin(); it != widgets.cend(); ++it) {
        const auto &current = it.value();
        // The plugin's id depends on where it was installed from ("local/…", "<author>/…")
        if (!current.value("type").endsWith("/spun-widget:spun")) continue;
        NoctaliaFrame frame;
        frame.output = current.value("output");
        frame.cx = current.value("cx").toDouble(); frame.cy = current.value("cy").toDouble();
        frame.width = current.value("box_width").toDouble(); frame.height = current.value("box_height").toDouble();
        frame.placementWidth = current.value("placement_width").toDouble(); frame.placementHeight = current.value("placement_height").toDouble();
        frame.settings = settings.value(it.key());
        if (frame.width > 0 && frame.height > 0) return frame;
    }
    return std::nullopt;
}

// The scene is 530×530 logical units; the turntable occupies the 440 px square at (45, 74).
static constexpr int sceneSize = 530;

int main(int argc, char **argv) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);
    // Tooltips in the system language (English when there is no translation)
    QTranslator translator;
    if (translator.load(QLocale(), "spun-widget", "_", ":/i18n")) app.installTranslator(&translator);
    // Some Wayland/OpenGL integrations default to Qt's basic 16 ms animation
    // driver. Use the vsync-driven loop unless explicitly overridden.
    if (QGuiApplication::platformName().startsWith("wayland") && !qEnvironmentVariableIsSet("QSG_RENDER_LOOP"))
        qputenv("QSG_RENDER_LOOP", "threaded");
    app.setApplicationName("spun-widget"); app.setApplicationDisplayName("Spun Widget");
    app.setDesktopFileName("spun-widget");
    QFont font(QFontInfo(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).family()); font.setPixelSize(14);
    app.setFont(font);

    Theme theme;
    ActivePlayer player;
    DiscPresentation presentation;
    qmlRegisterType<PlayerBody>("SpunWidget", 1, 0, "PlayerBody");
    qmlRegisterType<Disc>("SpunWidget", 1, 0, "Disc");
    qmlRegisterType<ProgressRing>("SpunWidget", 1, 0, "ProgressRing");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("activePlayer", &player);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("presentation", &presentation);
    WindowState windowState;
    engine.rootContext()->setContextProperty("windowState", &windowState);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    windowState.attach(window);

    // Like Noctalia's desktop widgets: a bottom-layer surface under every window, on every
    // workspace, never taking keyboard focus (mouse only; Shift comes from Hyprland, see below).
    // (No layer outside Wayland, e.g. offscreen: then it is a plain window.)
    auto *layer = LayerShellQt::Window::get(window);
    if (layer) {
        layer->setScope(QStringLiteral("spun-widget"));
        layer->setLayer(LayerShellQt::Window::LayerBottom);
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        // Without a Noctalia frame the widget stays centred on the screen.
        layer->setAnchors({});
    }
    const auto fit = [window](double scale) {
        window->setProperty("widgetScale", scale);
        const int side = qRound(sceneSize * scale);
        window->resize(side, side);
        // Scale the scene, never QQuickWindow's input-coordinate root.
        if (auto *content = window->property("contentItem").value<QQuickItem *>()) {
            content->setTransformOrigin(QQuickItem::TopLeft);
            content->setScale(scale);
            content->setSize(QSizeF(sceneSize, sceneSize));
        }
        // Clicks outside the turntable fall through to the desktop.
        QPainterPath housing; housing.addRoundedRect(QRectF(45, 74, 440, 440), 28, 28);
        QTransform zoom; zoom.scale(scale, scale);
        window->setMask(zoom.map(QRegion(housing.toFillPolygon().toPolygon())));
    };
    const auto place = [window, layer, &app, fit] {
        const auto frame = noctaliaFrame();
        // Missing settings fall back to the defaults in Main.qml (the plugin's defaults)
        QVariantMap settings = frame ? frame->settings : QVariantMap();
        settings.insert(publishedSettings());
        window->setProperty("widgetSettings", settings);
        if (!frame || !layer) { if (layer) layer->setAnchors({}); fit(1); return; }
        QScreen *screen = window->screen();
        for (auto *candidate : app.screens()) if (candidate->name() == frame->output) screen = candidate;
        const QRect area = screen ? screen->geometry() : QRect();
        // Noctalia rescales widgets when the output size differs from the one they were placed on
        const double fx = frame->placementWidth > 0 && area.width() > 0 ? area.width() / frame->placementWidth : 1;
        const double fy = frame->placementHeight > 0 && area.height() > 0 ? area.height() / frame->placementHeight : 1;
        const double scale = qBound(0.2, qMin(frame->width * fx, frame->height * fy) / 440.0, 3.0);
        fit(scale);
        const double left = frame->cx * fx - 220 * scale - 45 * scale;
        const double top = frame->cy * fy - 220 * scale - 74 * scale;
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
        layer->setMargins(QMargins(qRound(left), qRound(top), 0, 0));
        // A paused, static widget draws no frames; commit the new position now
        window->requestUpdate();
    };
    place();
    // Noctalia rewrites its settings file when a widget is moved; follow it live.
    auto *watcher = new QFileSystemWatcher(window);
    auto *debounce = new QTimer(window);
    debounce->setSingleShot(true); debounce->setInterval(150);
    QObject::connect(debounce, &QTimer::timeout, window, place);
    const auto watch = [watcher] {
        for (const auto &path : {noctaliaSettingsPath(), publishedSettingsPath()}) {
            if (path.isEmpty()) continue;
            if (!watcher->files().contains(path) && QFile::exists(path)) watcher->addPath(path);
            const auto dir = QFileInfo(path).absolutePath();
            if (QFileInfo::exists(dir) && !watcher->directories().contains(dir)) watcher->addPath(dir);
        }
    };
    watch();
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, window, [watch, debounce] { watch(); debounce->start(); });
    QObject::connect(watcher, &QFileSystemWatcher::directoryChanged, window, [watch, debounce] { watch(); debounce->start(); });

    // Without keyboard focus Shift (needle precision and snapping) is reported by Hyprland,
    // which writes "1"/"0" to $XDG_RUNTIME_DIR/spun-shift (see hyprland.lua).
    const QString shiftPath = qEnvironmentVariable("XDG_RUNTIME_DIR", "/tmp") + "/spun-shift";
    auto *shiftWatcher = new QFileSystemWatcher(window);
    const auto readShift = [window, shiftWatcher, shiftPath] {
        if (!shiftWatcher->files().contains(shiftPath) && QFile::exists(shiftPath)) shiftWatcher->addPath(shiftPath);
        QFile file(shiftPath);
        window->setProperty("shiftHeld", file.open(QIODevice::ReadOnly) && file.read(1) == "1");
    };
    shiftWatcher->addPath(QFileInfo(shiftPath).absolutePath());
    readShift();
    QObject::connect(shiftWatcher, &QFileSystemWatcher::fileChanged, window, readShift);
    QObject::connect(shiftWatcher, &QFileSystemWatcher::directoryChanged, window, readShift);
    window->show();

    // Noctalia removes its desktop widgets while its widget editor is open and creates them again
    // afterwards (also when it restarts). The recreated transparent frame of this widget then lies
    // above the player and swallows its clicks, so when Hyprland reports that frame's layer, the
    // player maps its own surface again to get back on top.
    auto *events = new QLocalSocket(window);
    auto *raise = new QTimer(window);
    raise->setSingleShot(true); raise->setInterval(100);
    QObject::connect(raise, &QTimer::timeout, window, [window, place] { window->hide(); window->show(); place(); });
    QObject::connect(events, &QLocalSocket::readyRead, window, [events, raise] {
        while (events->canReadLine())
            if (const auto line = events->readLine(); line.startsWith("openlayer>>noctalia-desktop-widget-") && line.contains("/spun-widget:spun")) raise->start();
    });
    events->connectToServer(qEnvironmentVariable("XDG_RUNTIME_DIR") + "/hypr/" + qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket2.sock");
    return app.exec();
}

#include "main.moc"
