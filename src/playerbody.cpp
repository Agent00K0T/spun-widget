#include "playerbody.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QRandomGenerator>

void PlayerBody::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    p->scale(width()/440.,height()/440.);
    const auto metal=[&](const QRectF &r,qreal radius){
        // Broad face, rounded bevel and a dark underside share one light source.
        const QColor base=m_surface;
        p->setPen(Qt::NoPen);p->setBrush(base.darker(210));
        p->drawRoundedRect(r,radius,radius);
        QLinearGradient edge(r.topLeft(),r.bottomRight());
        edge.setColorAt(0,base.lighter(165));edge.setColorAt(.26,base.lighter(115));
        edge.setColorAt(.64,base.darker(160));edge.setColorAt(1,base.darker(230));
        p->setBrush(edge);p->drawRoundedRect(r.adjusted(.7,.5,-.7,-1.7),radius,radius);
        const QRectF face=r.adjusted(2,2,-2,-5);
        QLinearGradient g(face.topLeft(),face.bottomRight());
        g.setColorAt(0,base.lighter(130));g.setColorAt(.35,base.lighter(112));
        g.setColorAt(1,base.darker(115));
        p->setBrush(g);p->drawRoundedRect(face,qMax(1.,radius-2),qMax(1.,radius-2));
        p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(245,249,255,45),.65));
        p->drawRoundedRect(r.adjusted(1,1,-1,-2),radius,radius);
    };
    const auto screw=[&](QPointF at){
        p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,135));p->drawEllipse(at+QPointF(0,.8),4.4,4.4);
        QConicalGradient g(at,35);g.setColorAt(0,QColor("#c4c7c6"));g.setColorAt(.25,QColor("#4a4f51"));g.setColorAt(.55,QColor("#b0b7b9"));g.setColorAt(.8,QColor("#303639"));g.setColorAt(1,QColor("#c4c7c6"));
        p->setPen(QPen(QColor(0,0,0,170),.65));p->setBrush(g);p->drawEllipse(at,3.1,3.1);
        p->setPen(QPen(QColor(0,0,0,210),.85));
        p->drawLine(at+QPointF(-1.5,1.5),at+QPointF(1.5,-1.5));
        p->drawLine(at+QPointF(-1.5,-1.5),at+QPointF(1.5,1.5));
    };
    const auto hinge=[&](QRectF r){
        QLinearGradient chrome(r.topLeft(),r.bottomLeft());
        chrome.setColorAt(0,QColor("#262b2d"));chrome.setColorAt(.22,QColor("#c1c5c3"));
        chrome.setColorAt(.45,QColor("#747b7c"));chrome.setColorAt(.78,QColor("#303637"));chrome.setColorAt(1,QColor("#151a1b"));
        p->setPen(QPen(QColor(0,0,0,170),.6));p->setBrush(chrome);p->drawRoundedRect(r,2,2);
        p->setPen(QPen(QColor(0,0,0,120),.7));
        for(double x=r.left()+4;x<r.right();x+=5)p->drawLine(QPointF(x,r.top()+1),QPointF(x,r.bottom()-1));
    };
    if(m_part==2) {
        const QRectF r(44,44,352,352);
        p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,100));p->drawRoundedRect(r.translated(2,5),5,5);
        metal(r,2);
        const QRectF cover=r.adjusted(5,5,-5,-5);
        if(!m_artwork.isNull()) {
            const auto side=qMin(m_artwork.width(),m_artwork.height());
            p->drawImage(cover,m_artwork,QRectF((m_artwork.width()-side)/2.,(m_artwork.height()-side)/2.,side,side));
        }
        p->setPen(QPen(QColor(0,0,0,65),1));p->drawLine(r.topRight()-QPointF(2,0),r.bottomRight()-QPointF(2,0));
        QLinearGradient gloss(r.topLeft(),r.bottomRight());gloss.setColorAt(0,QColor(255,255,255,12));gloss.setColorAt(.48,Qt::transparent);gloss.setColorAt(.5,QColor(255,255,255,4));gloss.setColorAt(1,Qt::transparent);
        p->fillRect(r.adjusted(1,1,-1,-1),gloss);
        return;
    }
    if(m_part==1) {
        QLinearGradient acrylic(0,8,0,63);acrylic.setColorAt(0,QColor(222,236,242,18));acrylic.setColorAt(1,QColor(204,225,237,2));
        p->setBrush(acrylic);p->setPen(QPen(QColor(220,235,243,48),.8));p->drawRoundedRect(QRectF(15,9,410,51),7,7);
        hinge(QRectF(69,9,24,7));hinge(QRectF(347,9,24,7));
        // The spindle belongs to the deck and remains stationary above the label.
        QRadialGradient spindle(QPointF(218.5,217),6);spindle.setColorAt(0,QColor("#e6ebed"));spindle.setColorAt(.5,QColor("#a5abad"));spindle.setColorAt(.8,QColor("#50585d"));spindle.setColorAt(1,QColor("#20272b"));
        p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,125));p->drawEllipse(QPointF(221,222),6,6);
        p->setBrush(spindle);p->drawEllipse(QPointF(220,220),4.5,5.5);
        return;
    }
    const QRectF body(6,6,428,428);
    p->setPen(Qt::NoPen);
    for(int i=6;i>0;--i){p->setBrush(QColor(0,0,0,7));p->drawRoundedRect(body.adjusted(-i,-i,i,i).translated(0,3),23,23);}
    metal(body,22);
    p->save();QPainterPath face;face.addRoundedRect(body.adjusted(3,3,-3,-7),20,20);p->setClipPath(face);
    QRandomGenerator grain(317);
    for(int i=0;i<2600;++i){
        const double x=body.left()+grain.generateDouble()*body.width(),y=body.top()+grain.generateDouble()*body.height();
        p->setPen(QPen(i%3?QColor(255,255,255,7):QColor(0,0,0,18),.45));
        p->drawLine(QPointF(x,y),QPointF(x+3.5,y));
    }
    // A broad reflected light distinguishes the face from its rounded sidewall.
    QRadialGradient light(QPointF(70,30),390);light.setColorAt(0,QColor(237,244,255,16));light.setColorAt(1,Qt::transparent);
    p->fillPath(face,light);p->restore();
    QRadialGradient recess(QPointF(220,220),197);recess.setColorAt(0,QColor("#16181b"));recess.setColorAt(.95,QColor("#16181b"));recess.setColorAt(.98,QColor("#090b0d"));recess.setColorAt(1,QColor(0,0,0,0));
    p->setPen(Qt::NoPen);p->setBrush(recess);p->drawEllipse(QPointF(220,220),198,198);
    p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor("#656a6b"),2));p->drawEllipse(QPointF(220,220),187,187);
    p->setPen(QPen(QColor("#181b1d"),5));p->drawEllipse(QPointF(220,220),183,183);
    for(int r:{185,188,191})for(int i=0;i<120;++i){p->save();p->translate(220,220);p->rotate(i*3+(r%2)*1.5);p->setPen(Qt::NoPen);p->setBrush(QColor(185,192,193,r==188?100:50));p->drawEllipse(QPointF(0,-r),.7,1.05);p->restore();}
    p->setPen(QPen(QColor(221,231,236,42),.8));p->setBrush(Qt::NoBrush);p->drawEllipse(QPointF(220,220),193,193);
    screw(QPointF(20,20));screw(QPointF(420,20));screw(QPointF(20,420));screw(QPointF(420,420));
    metal(QRectF(37,391,34,8),4);metal(QRectF(365,31,25,11),3);
}
