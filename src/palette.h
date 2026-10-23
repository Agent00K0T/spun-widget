#pragma once
#include <QColor>
#include <QImage>
#include <QList>

// Main colours of a cover, heaviest first. Its pixels are sorted into twelve hue bins plus one for
// greys, colourful pixels counting more; each next colour is the heaviest bin at least 60° in hue
// away from the ones taken (the greys count as far from every hue). Bins under 8% of the heaviest
// are ignored. Empty for an empty or all black/white image.
QList<QColor> coverColors(const QImage &art, int count);
// Whether the colour came from the grey bin
bool isGrey(const QColor &colour);
