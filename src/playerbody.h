#pragma once
#include <QQuickPaintedItem>
#include <QImage>

// The turntable's static surfaces, cached while the scene graph moves the record.
// From Spun's src/playerbody.h (https://github.com/yappologistic/Spun), turntable only.
// part 0: the deck, 1: the acrylic lid strip and spindle above the record,
// 2: the album sleeve shown while records are swapped.
class PlayerBody : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QColor surface MEMBER m_surface NOTIFY changed)
    Q_PROPERTY(QImage artwork MEMBER m_artwork NOTIFY changed)
    Q_PROPERTY(int part MEMBER m_part NOTIFY changed)
public:
    explicit PlayerBody(QQuickItem *parent=nullptr):QQuickPaintedItem(parent) {
        setAntialiasing(true);connect(this,&PlayerBody::changed,this,[this]{update();});
    }
    void paint(QPainter *painter) override;
signals:
    void changed();
private:
    QColor m_surface=QColor("#343438");
    QImage m_artwork;
    int m_part=0;
};
