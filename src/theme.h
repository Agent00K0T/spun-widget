#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

// Read Noctalia's exported semantic colours; never modify the shell's theme.
// From Spun's src/theme.h (https://github.com/yappologistic/Spun), reduced to what the widget uses.
class Theme : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
  Q_PROPERTY(qreal radius READ radius NOTIFY changed)
  Q_PROPERTY(qreal motionScale READ motionScale NOTIFY changed)
public:
  explicit Theme(QObject *parent = nullptr);
  QVariantMap colors() const { return m_colors; }
  qreal radius() const { return m_radius; }
  qreal motionScale() const { return m_motion; }
  void reload();
signals:
  void changed();

private:
  void watch();
  QString m_config, m_state;
  QVariantMap m_colors;
  qreal m_radius = 1, m_motion = 1;
  QFileSystemWatcher m_watcher;
  QTimer m_debounce;
};
