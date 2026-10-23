#include "disc.h"
#include "palette.h"
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <cmath>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

Disc::Disc(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &Disc::tintChanged, this, [this] { update(); });
}
void Disc::setArtwork(const QImage &image) { if (m_art.cacheKey() == image.cacheKey()) return; m_art = image; update(); emit artworkChanged(); }
void Disc::setOverlay(bool value) { if (m_overlay == value) return; m_overlay = value; update(); emit overlayChanged(); }

// The label without a cover (nothing playing, or no artwork). One immutable image shared by all records.
static const QImage &placeholderArt() {
    static const QImage image = [] {
        QImage image(512,512,QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&image);
        QLinearGradient finish(0,0,512,512);
        finish.setColorAt(0,QColor("#44474d"));finish.setColorAt(1,QColor("#2b2d33"));
        painter.fillRect(image.rect(),finish);
        return image;
    }();
    return image;
}

// Coloured vinyl: the cover's main colour (its heaviest colourful bin, see coverColors), dark
// enough for the grooves, the sheen and the lighter chapter rings to stay readable. Invalid for a
// grey cover or none: the record stays black.
static QColor vinylTint(const QColor &colour) {
    return QColor::fromHslF(qMax(0.f, colour.hslHueF()), qMin(.75f, colour.hslSaturationF()), .2f);
}
static QColor vinylTint(const QImage &art) {
    for (const auto &colour : coverColors(art, 3))
        if (!isGrey(colour)) return vinylTint(colour);
    return {};
}

void Disc::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double side = qMin(width(), height());
    p->translate((width() - side) / 2, (height() - side) / 2);
    p->scale(side / 1000, side / 1000);
    const QPointF center(500,500);
    QPainterPath record; record.setFillRule(Qt::OddEvenFill);
    record.addEllipse(center,497,497); record.addEllipse(center,16,16);
    p->setClipPath(record);
    if (m_overlay) {
        // Fixed light over a rotating, cached record texture; no per-frame painting.
        QConicalGradient sheen(center,24);
        sheen.setColorAt(0,Qt::transparent); sheen.setColorAt(.07,Qt::transparent);
        sheen.setColorAt(.12,QColor(203,216,229,37));sheen.setColorAt(.155,QColor(203,216,229,10));
        sheen.setColorAt(.22,Qt::transparent); sheen.setColorAt(.5,Qt::transparent);
        sheen.setColorAt(.61,QColor(244,233,214,28));sheen.setColorAt(.67,QColor(244,233,214,5));
        sheen.setColorAt(.73,Qt::transparent); sheen.setColorAt(1,Qt::transparent);
        p->fillPath(record,sheen); return;
    }
    // Only concentric shading: it looks the same however the record turns
    QRadialGradient body(center,500);
    // tintFromArt: the record's own cover (so a record keeps its colour while it is swapped out);
    // otherwise the tint colour, or black without one
    const QColor tint = m_tintFromArt ? vinylTint(m_art) : m_tint.isValid() && !isGrey(m_tint) ? vinylTint(m_tint) : QColor();
    if (tint.isValid()) {
        const auto shade = [&](double lightness) { return QColor::fromHslF(tint.hslHueF(), tint.hslSaturationF(), lightness); };
        body.setColorAt(0,shade(.17)); body.setColorAt(.46,shade(.2)); body.setColorAt(.6,shade(.18));
        body.setColorAt(.75,shade(.21)); body.setColorAt(.94,shade(.14)); body.setColorAt(1,shade(.27));
    } else {
        body.setColorAt(0,QColor("#141519")); body.setColorAt(.46,QColor("#17181c"));
        body.setColorAt(.94,QColor("#101114")); body.setColorAt(1,QColor("#24252a"));
    }
    p->fillPath(record,body);
    p->setBrush(Qt::NoBrush);
    QRandomGenerator grooveSpacing(44);
    for (double r=213;r<488;r+=2.6+grooveSpacing.generateDouble()*1.8) {
        p->setPen(QPen(QColor(180,185,195,12+grooveSpacing.bounded(16)),.65));p->drawEllipse(center,r,r);
        p->setPen(QPen(QColor(0,0,0,90),.9));p->drawEllipse(center,r+1.1,r+1.1);
    }
    p->setPen(QPen(QColor(1,2,3,180),3));p->drawEllipse(center,492,492);
    p->setPen(QPen(QColor(150,163,174,55),.8));p->drawEllipse(center,495,495);
    for(int r:{242,310,381,452}) { p->setPen(QPen(QColor(2,3,5,115),3));p->drawEllipse(center,r,r); }
    const double radius=198;
    QPainterPath label; label.addEllipse(center,radius,radius);
    p->save();p->setClipPath(label,Qt::IntersectClip);
    const auto &art=m_art.isNull()?placeholderArt():m_art;const double crop=qMin(art.width(),art.height());
    p->drawImage(QRectF(500-radius,500-radius,radius*2,radius*2),art,QRectF((art.width()-crop)/2,(art.height()-crop)/2,crop,crop));
    p->restore();
    p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,120),3));p->drawEllipse(center,radius+1,radius+1);
    // Pressed paper has a shallow concentric label ridge around the spindle.
    p->setPen(QPen(QColor(0,0,0,85),1.8));p->drawEllipse(center,72,72);
    p->setPen(QPen(QColor(255,255,255,28),.9));p->drawEllipse(center,74,74);
    p->setPen(QPen(QColor(255,255,255,32),1));p->drawEllipse(center,178,178);
    p->setPen(QPen(QColor(0,0,0,170),3));p->drawEllipse(center,18,18);
    p->setPen(QPen(QColor(255,255,255,40),1));p->drawEllipse(center,21,21);
}

class RingNode final : public QSGGeometryNode {
public:
    int capacity = 0, indexCapacity = 0, indexedSteps = -1;
    struct Sample { qreal sine, cosine, waveSine, waveCosine, envelope; };
    QList<Sample> samples;
    qreal sampledArc = -1, sampledRadius = -1;
};

// Small vertex buffers animate the rim without repainting or uploading a full record-sized texture.
QSGNode *ProgressRing::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<RingNode *>(oldNode);
    if (!node) {
        node = new RingNode;
        node->setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0, 0, QSGGeometry::UnsignedIntType));
        node->geometry()->setIndexDataPattern(QSGGeometry::StaticPattern);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    const qreal radius = qMin(width(), height())/2-6;
    const qreal arc = 2*M_PI*m_progress;
    const int steps = qMax(1, int(std::ceil(radius*arc/2)));
    constexpr int dotSteps = 24;
    auto *geometry = node->geometry();
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    const int ringVertices = (steps+1)*4;
    const int count = ringVertices + dotSteps*9;
    const int indexCount = steps*18 + dotSteps*9;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    // Qt 6.10 can change the active vertex count without reallocating storage.
    if (count > node->capacity || indexCount > node->indexCapacity) {
        node->capacity = qMax(node->capacity, ((count + 255) / 256) * 256);
        node->indexCapacity = qMax(node->indexCapacity, ((indexCount + 255) / 256) * 256);
        geometry->allocate(node->capacity, node->indexCapacity);
        node->indexedSteps = -1;
    }
    geometry->setVertexCount(count);
    geometry->setIndexCount(indexCount);
#else
    geometry->allocate(count, indexCount);
    node->indexedSteps = -1;
#endif
    // Adjacent triangles share the exact same edge vertices. Their topology
    // changes only when the segment count changes, not as the wave animates.
    if (node->indexedSteps != steps) {
        auto *indices = geometry->indexDataAsUInt();
        for (int i=0; i<steps; ++i) for (int band=0; band<3; ++band) {
            const uint a=i*4+band, b=(i+1)*4+band;
            for (uint index : {a,b,a+1,a+1,b,b+1}) *indices++ = index;
        }
        for (int i=0; i<dotSteps*9; ++i) *indices++ = ringVertices+i;
        node->indexedSteps = steps;
        geometry->markIndexDataDirty();
    }
    auto *vertices = geometry->vertexDataAsColoredPoint2D();
    const QPointF center(width()/2, height()/2);
    const qreal alpha = m_accent.alphaF();
    const int red = qRound(m_accent.red()*alpha), green = qRound(m_accent.green()*alpha);
    const int blue = qRound(m_accent.blue()*alpha), opaque = qRound(255*alpha);
    auto vertex = [&](QPointF point, bool visible) {
        (vertices++)->set(point.x(), point.y(), visible ? red : 0, visible ? green : 0,
                         visible ? blue : 0, visible ? opaque : 0);
    };
    // Progress changes less often than the animated phase. Reuse the expensive
    // trigonometry between progress updates, retaining the same tessellation.
    if (node->samples.size() != steps+1 || node->sampledArc != arc || node->sampledRadius != radius) {
        node->samples.resize(steps+1);
        for (int i=0; i<=steps; ++i) {
            const qreal t=arc*i/steps, wave=t*28;
            node->samples[i]={std::sin(t),-std::cos(t),std::sin(wave),std::cos(wave),qBound(0.,qMin(t,arc-t)*radius/14,1.)};
        }
        node->sampledArc=arc;node->sampledRadius=radius;
    }
    const qreal phaseSine=std::sin(m_phase),phaseCosine=std::cos(m_phase);
    constexpr qreal offsets[] = {-1.9, -.9, .9, 1.9};
    constexpr bool opacity[] = {false, true, true, false};
    for (const auto &sample : std::as_const(node->samples)) {
        const qreal wave=(sample.waveSine*phaseCosine-sample.waveCosine*phaseSine)*m_amplitude*sample.envelope;
        for (int band=0; band<4; ++band) {
            const qreal r=radius+wave+offsets[band];
            vertex(center+QPointF(sample.sine*r,sample.cosine*r),opacity[band]);
        }
    }
    const QPointF head=center+QPointF(std::sin(arc)*radius,-std::cos(arc)*radius);
    for (int i=0; i<dotSteps; ++i) {
        const qreal a=2*M_PI*i/dotSteps, b=2*M_PI*(i+1)/dotSteps;
        const QPointF av(std::cos(a),std::sin(a)), bv(std::cos(b),std::sin(b));
        vertex(head,1); vertex(head+av*2.7,1); vertex(head+bv*2.7,1);
        vertex(head+av*2.7,1); vertex(head+av*3.5,0); vertex(head+bv*2.7,1);
        vertex(head+bv*2.7,1); vertex(head+av*3.5,0); vertex(head+bv*3.5,0);
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

void DiscPresentation::releaseOutgoing() {
    if (m_outgoing.isNull()) return;
    m_outgoing = {};
    emit changed();
}
void DiscPresentation::present(const QImage &image,const QString &key,bool animate) {
    // Only the outgoing animation may retain a previous cover. The current
    // record must not borrow artwork while a new track is loading.
    const QImage art=key.isEmpty()?QImage():image;
    const bool swap=!key.isEmpty() && !m_key.isEmpty() && key!=m_key && animate;
    const bool clearedOutgoing=(!animate || key.isEmpty()) && !m_outgoing.isNull();
    if(swap)m_outgoing=m_art;
    else if(clearedOutgoing)m_outgoing={};
    if (!swap && !clearedOutgoing && m_key == key && m_art.cacheKey() == art.cacheKey()) return;
    m_art=art; m_key=key; emit changed();
    if(swap)emit swapRequested();
}
