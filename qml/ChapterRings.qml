import QtQuick

// Chapters of a YouTube video as thin rings at the chapter starts, outer chapter first, in two
// colours of the video's cover; the record itself stays black. Only the ring where the current
// chapter starts is lit, fading over to the next one. Radius where the stylus touches the record:
// the tonearm pivot sits at (158, -120) from the record centre with a 197 px arm (Tonearm.qml),
// so the needle lands on its ring.
Canvas {
    id: rings
    property var chapters: []
    property real duration: 0
    // ms: the chapter it falls in is lit
    property real at: 0
    // Ring colours at the rim side and towards the label
    property color outer
    property color inner
    property real needleStart: 0
    property real needleRange: 0
    property bool animate: false
    property int fadeTime: 600
    readonly property int current: {
        let index = -1
        for (let i = 0; i < chapters.length; ++i) if (chapters[i].start <= at) index = i
        return index
    }
    onChaptersChanged: requestPaint()
    onDurationChanged: requestPaint()
    onOuterChanged: requestPaint()
    onInnerChanged: requestPaint()
    property int lit: -1
    property int unlit: -1
    property real fade: 1
    NumberAnimation on fade { id: litFade; running: false; from: 0; to: 1; duration: rings.fadeTime; easing.type: Easing.InOutQuad }
    onCurrentChanged: { unlit = lit; lit = current; if (animate) litFade.restart(); else fade = 1; requestPaint() }
    onFadeChanged: requestPaint()
    Component.onCompleted: lit = current
    function glow(i) { return (i === lit ? fade : 0) + (i === unlit ? 1 - fade : 0) }
    function radiusAt(ms) {
        const angle = (needleStart + needleRange * Math.max(0, Math.min(1, ms / duration))) * Math.PI / 180
        return Math.hypot(158 - 197 * Math.sin(angle), -120 + 197 * Math.cos(angle))
    }
    // From the outer colour at the rim to the inner one towards the label
    function shade(t, alpha) {
        return Qt.rgba(outer.r + (inner.r - outer.r) * t, outer.g + (inner.g - outer.g) * t,
                       outer.b + (inner.b - outer.b) * t, alpha)
    }
    onPaint: {
        const ctx = getContext("2d"); ctx.reset()
        const count = chapters.length
        if (duration <= 0) return
        for (let i = 1; i < count; ++i) {
            const g = glow(i)
            ctx.strokeStyle = shade((i - 1) / Math.max(1, count - 2), .75 + .25 * g)
            ctx.lineWidth = 1.2 + .8 * g
            ctx.beginPath(); ctx.arc(width / 2, height / 2, radiusAt(chapters[i].start), 0, 2 * Math.PI); ctx.stroke()
        }
    }
}
