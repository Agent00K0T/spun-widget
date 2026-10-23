#include "theme.h"
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

static QString read(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll())
                                        : QString();
}
Theme::Theme(QObject *parent)
    : QObject(parent),
      m_config(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)),
      m_state(QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation)) {
  m_debounce.setSingleShot(true);
  m_debounce.setInterval(90);
  connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
          [this] { m_debounce.start(); });
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this] { m_debounce.start(); });
  connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
  reload();
}
void Theme::watch() {
  // Watch parent directories too: generated files are often atomically
  // replaced.
  QStringList paths{m_config,
                    m_config + "/gtk-4.0",
                    m_config + "/gtk-4.0/noctalia.css",
                    m_config + "/noctalia",
                    m_config + "/noctalia/config.toml",
                    m_state,
                    m_state + "/noctalia",
                    m_state + "/noctalia/settings.toml"};
  for (const auto &path : paths)
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path) &&
        !m_watcher.directories().contains(path))
      m_watcher.addPath(path);
}
void Theme::reload() {
  watch();
  const auto previous = m_colors;
  const qreal oldRadius = m_radius, oldMotion = m_motion;
  QMap<QString, QColor> tokens;
  const QString css = read(m_config + "/gtk-4.0/noctalia.css");
  static const QRegularExpression expression(
      "@define-color\\s+(\\w+)\\s+(#[0-9a-fA-F]{6})\\s*;");
  auto matches = expression.globalMatch(css);
  while (matches.hasNext()) {
    const auto match = matches.next();
    tokens.insert(match.captured(1), QColor(match.captured(2)));
  }
  // Keep the last complete palette while Noctalia writes/regenerates its
  // export.
  const bool complete = tokens.contains("window_fg_color") &&
                        tokens.contains("accent_bg_color") &&
                        tokens.contains("card_bg_color");
  if (complete || m_colors.isEmpty()) {
    m_colors = {
        {"text", tokens.value("window_fg_color", QColor("#ededed"))},
        {"card", tokens.value("card_bg_color", QColor("#262626"))},
        {"accent", tokens.value("accent_bg_color", QColor("#b8c4cf"))}};
  }
  m_radius = m_motion = 1;
  bool animations = true;
  static const QRegularExpression setting("^(\\w+)\\s*=\\s*(.+)$");
  for (const auto &path : QStringList{m_config + "/noctalia/config.toml",
                                      m_state + "/noctalia/settings.toml"}) {
    QString section;
    for (QString line : read(path).split('\n')) {
      line = line.trimmed();
      if (line.startsWith('[')) {
        section = line;
        continue;
      }
      if (section != "[shell]" && section != "[shell.animation]")
        continue;
      auto match = setting.match(line);
      if (!match.hasMatch())
        continue;
      const QString key = match.captured(1),
                    value = match.captured(2).split('#').first().trimmed();
      bool ok = false;
      qreal number = value.toDouble(&ok);
      if (section == "[shell]" && key == "corner_radius_scale" && ok)
        m_radius = qBound(0., number, 2.);
      if (section == "[shell.animation]" && key == "speed" && ok)
        m_motion = qBound(.1, number, 3.);
      if (section == "[shell.animation]" && key == "enabled")
        animations = value != "false";
    }
  }
  if (!animations)
    m_motion = 0;
  if (previous != m_colors || oldRadius != m_radius || oldMotion != m_motion)
    emit changed();
}
