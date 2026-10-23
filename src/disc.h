#pragma once
#include <QQuickPaintedItem>
#include <QImage>

// From Spun's src/disc.h (https://github.com/yappologistic/Spun), vinyl only.

// A vinyl record with the cover as its label. With overlay set it paints only the fixed
// light that passes over the rotating record.
class Disc : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QImage artwork READ artwork WRITE setArtwork NOTIFY artworkChanged)
    Q_PROPERTY(bool overlay READ overlay WRITE setOverlay NOTIFY overlayChanged)
    // Coloured vinyl: from the cover, or from the tint colour (invalid or grey: black)
    Q_PROPERTY(bool tintFromArt MEMBER m_tintFromArt NOTIFY tintChanged)
    Q_PROPERTY(QColor tint MEMBER m_tint NOTIFY tintChanged)
public:
    explicit Disc(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    QImage artwork() const { return m_art; }
    void setArtwork(const QImage &image);
    bool overlay() const { return m_overlay; }
    void setOverlay(bool value);
signals:
    void artworkChanged();
    void overlayChanged();
    void tintChanged();
private:
    QImage m_art;
    bool m_overlay = false, m_tintFromArt = false;
    QColor m_tint;
};

// The progress arc around the record, a wave while playing.
class ProgressRing : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY changed)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY changed)
    Q_PROPERTY(qreal amplitude READ amplitude WRITE setAmplitude NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY changed)
public:
    explicit ProgressRing(QQuickItem *parent = nullptr) : QQuickItem(parent) { setFlag(ItemHasContents); }
    qreal progress() const { return m_progress; }
    qreal phase() const { return m_phase; }
    qreal amplitude() const { return m_amplitude; }
    QColor accent() const { return m_accent; }
    void setProgress(qreal v) { v=qBound(0.,v,1.); if (m_progress==v) return; m_progress=v; update(); emit changed(); }
    void setPhase(qreal v) { if (m_phase==v) return; m_phase=v; update(); emit changed(); }
    void setAmplitude(qreal v) { if (m_amplitude==v) return; m_amplitude=v; update(); emit changed(); }
    void setAccent(QColor v) { if (m_accent==v) return; m_accent=v; update(); emit changed(); }
signals:
    void changed();
protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
private:
    qreal m_progress=0, m_phase=0, m_amplitude=0;
    QColor m_accent;
};

// Holds the previous artwork while the scene graph exchanges two records.
class DiscPresentation : public QObject {
    Q_OBJECT
    Q_PROPERTY(QImage artwork READ artwork NOTIFY changed)
    Q_PROPERTY(QImage outgoing READ outgoing NOTIFY changed)
public:
    explicit DiscPresentation(QObject *parent=nullptr) : QObject(parent) {}
    QImage artwork() const { return m_art; }
    QImage outgoing() const { return m_outgoing; }
    Q_INVOKABLE void present(const QImage &art, const QString &key, bool animate);
    Q_INVOKABLE void releaseOutgoing();
signals:
    void changed();
    void swapRequested();
private:
    QImage m_art, m_outgoing;
    QString m_key;
};
