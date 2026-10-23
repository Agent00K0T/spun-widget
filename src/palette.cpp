#include "palette.h"

bool isGrey(const QColor &colour) { return colour.hslSaturationF() < .18 || colour.lightnessF() < .12; }

QList<QColor> coverColors(const QImage &art, int count) {
    if (art.isNull()) return {};
    const QImage small = art.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    struct Bin { double weight = 0, r = 0, g = 0, b = 0; };
    Bin bins[13];   // 0..11 hues, 12 greys
    for (int y = 0; y < small.height(); ++y)
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = QColor::fromRgb(small.pixel(x, y));
            const double l = c.lightnessF();
            if (l < .06 || l > .97) continue;   // black bars and blown-out white say nothing
            const bool grey = isGrey(c);
            Bin &bin = bins[grey ? 12 : qBound(0, int(c.hslHueF() * 12), 11)];
            const double weight = grey ? 1 : 1 + 3 * c.hslSaturationF();
            bin.weight += weight; bin.r += c.redF() * weight; bin.g += c.greenF() * weight; bin.b += c.blueF() * weight;
        }
    QList<int> taken;
    const auto farEnough = [&](int i) {
        for (int t : taken) {
            const int distance = t == 12 || i == 12 ? 6 : qMin((i - t + 12) % 12, (t - i + 12) % 12);
            if (distance < 2) return false;
        }
        return true;
    };
    while (taken.size() < count) {
        int best = -1;
        for (int i = 0; i < 13; ++i) {
            if (bins[i].weight <= 0 || taken.contains(i) || !farEnough(i)) continue;
            if (!taken.isEmpty() && bins[i].weight < bins[taken.first()].weight * .08) continue;
            if (best < 0 || bins[i].weight > bins[best].weight) best = i;
        }
        if (best < 0) break;
        taken.append(best);
    }
    QList<QColor> colours;
    for (int i : taken) {
        const Bin &b = bins[i];
        colours.append(QColor::fromRgbF(b.r / b.weight, b.g / b.weight, b.b / b.weight));
    }
    return colours;
}
