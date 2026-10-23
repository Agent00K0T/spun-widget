import QtQuick
import QtQuick.Controls
import SpunWidget

// The turntable of Spun (qml/Main.qml, https://github.com/yappologistic/Spun) as a desktop widget.
// A 530×530 scene: the deck occupies the 440 px square at (45, 74).
ApplicationWindow {
    id: root
    title: "Spun Widget"
    // Shown from C++ once its desktop layer is configured
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    // Set from C++: the fit to the Noctalia frame, and Shift as reported by Hyprland
    property real widgetScale: 1
    property bool shiftHeld: false
    // The widget's settings from Noctalia's widget editor (declared in the plugin's plugin.toml);
    // a missing value means the plugin's default
    property var widgetSettings: ({})
    readonly property string recordColor: widgetSettings.record_color || "cover"
    readonly property string chapterColor: widgetSettings.chapter_color || "cover"
    readonly property string progressColor: widgetSettings.progress_color || "system"
    readonly property bool swapAnimation: widgetSettings.swap_animation !== false
    Binding { target: activePlayer; property: "chaptersEnabled"; value: root.widgetSettings.chapters !== false }
    Binding { target: activePlayer; property: "playerFilter"; value: root.widgetSettings.player || "" }
    // Colour modes: "default" = the player's own colours (Spun's built-in palette, black vinyl),
    // "system" = Noctalia's theme, "cover" = the colours of the track's cover
    readonly property color standardAccent: "#b8c4cf"
    function modeColor(mode, cover) { return mode === "cover" ? cover : mode === "system" ? root.accent : root.standardAccent }
    readonly property real uiScale: widgetScale
    width: Math.round(530 * uiScale); height: Math.round(530 * uiScale)
    onUiScaleChanged: fitOverlay()
    Component.onCompleted: { fitOverlay(); ringProgress = recordVisualProgress; Qt.callLater(presentDisc) }
    // Popups share the logical scene; their overlay must use the same scale.
    function fitOverlay() {
        Overlay.overlay.transformOrigin = Item.TopLeft
        Overlay.overlay.scale = uiScale
        Overlay.overlay.x = 0; Overlay.overlay.y = 0
        Overlay.overlay.width = 530; Overlay.overlay.height = 530
    }
    Connections {
        target: Overlay.overlay
        function onXChanged() { if (target.x !== 0) target.x = 0 }
        function onYChanged() { if (target.y !== 0) target.y = 0 }
        function onWidthChanged() { if (target.width !== 530) target.width = 530 }
        function onHeightChanged() { if (target.height !== 530) target.height = 530 }
    }

    readonly property var player: activePlayer
    // The player's own body: Spun's built-in grey or the Noctalia theme's card colour
    readonly property string playerColor: widgetSettings.player_color || "system"
    readonly property color surface: playerColor === "default" ? "#262626" : theme.colors.card
    // The tint of the arm's brass follows the same choice
    readonly property color armAccent: playerColor === "default" ? standardAccent : accent
    readonly property color accent: theme.colors.accent
    readonly property bool animate: Style.motion
    // The record sits a little smaller inside the deck
    readonly property real mediumScale: .86

    // Needle geometry: music starts after a visible lead-in (92% of the radius), the needle
    // rests off the record. See Tonearm.qml.
    readonly property real needleStart: -4
    readonly property real needleRange: 30
    readonly property real needleRest: -14
    // The stylus reaches the record's edge here: from it to needleStart lies the lead-in, where a
    // dropped needle plays the track from the start
    readonly property real needleEdge: -9.3

    // YouTube chapter starts as fractions of the video (rings, needle snapping, seek labels).
    // A single "chapter" covering the whole video means the video has no chapters.
    readonly property var chapterFractions: player.duration > 0 && player.chapters.length > 1
        ? player.chapters.map(chapter => chapter.start / player.duration) : []
    function nearestChapterFraction(fraction) {
        let best = fraction, distance = 2
        for (const start of chapterFractions) if (Math.abs(start - fraction) < distance) { distance = Math.abs(start - fraction); best = start }
        return best
    }
    function chapterAt(ms) {
        if (!chapterFractions.length) return ""
        let title = ""
        for (const chapter of player.chapters) if (chapter.start <= ms) title = chapter.title
        return title
    }
    function time(ms) {
        let sec = Math.max(0, Math.floor(ms / 1000))
        return Math.floor(sec / 60).toString().padStart(2,"0") + ":" + (sec % 60).toString().padStart(2,"0")
    }

    // The player reports its position every 200 ms; while it plays the position is also taken on
    // every frame, so the ring and the arm move smoothly even on short tracks
    property real livePosition: player.position
    Connections { target: root.player; function onPositionChanged() { root.livePosition = root.player.position } }
    readonly property real progress: player.duration > 0 ? livePosition / player.duration : 0
    // Seeking by anything but the tonearm (the rim). The arm keeps its own drag apart: a shared
    // flag still held the drag for a moment after release and made the arm jump.
    readonly property bool otherSeekPreview: scrubber.scrubbing
    // What the record shows: a preview while seeking, the drop point while the needle lands
    readonly property real recordVisualProgress: {
        if (scrubber.scrubbing) return scrubber.previewFraction
        if (tonearm.previewing) return tonearm.previewProgress
        if (tonearm.landing) return tonearm.landingProgress
        return progress
    }
    // The progress ring glides back (instead of jumping) when a needle drag ends without a seek.
    // It also glides when playback jumps by more than 2 s (YouTube resuming where the video was
    // left, a seek in the player), and when a drag of the needle or the rim takes over (the ring
    // moves to the point under it); during the drag it follows the hand directly.
    // The glide closes part of the remaining distance on every frame, so a target that keeps
    // moving (playback, a dragged needle) is caught up with instead of restarting an animation.
    property real ringProgress: 0
    property real ringTarget: 0
    property bool ringGliding: false
    property string ringSource: "player"
    function easeRing() { if (animate) ringGliding = true }
    function followRing() {
        const target = recordVisualProgress
        const source = scrubber.scrubbing ? "rim" : tonearm.previewing ? "needle" : "player"
        const jump = Math.abs(target - ringProgress) * player.duration > 2000
        if (jump && (source === "player" || source !== ringSource)) easeRing()
        ringSource = source
        ringTarget = target
        if (!ringGliding) ringProgress = target
    }
    onRecordVisualProgressChanged: followRing()
    FrameAnimation {
        running: root.ringGliding
        onTriggered: {
            // About 0.45 s to settle
            root.ringProgress += (root.ringTarget - root.ringProgress) * (1 - Math.exp(-frameTime / .09))
            if (Math.abs(root.ringTarget - root.ringProgress) < .0005) { root.ringProgress = root.ringTarget; root.ringGliding = false }
        }
    }

    // Record swap: a new album (or video) slides the old record out, its sleeve in, then the new record
    property real lidOpen: 0
    property real packageOpacity: 0
    property real swapOffset: 0
    property real outgoingOffset: 0
    property real outgoingOpacity: 0
    property real incomingOpacity: 1
    property real outgoingAngle: 0
    property var outgoingRings: ({})
    // Chapter ring and progress colours by mode; cover colours change over smoothly when a new cover arrives
    property color ringOuter: chapterColor === "cover" ? player.ringOuter : Qt.lighter(modeColor(chapterColor), 1.45)
    property color ringInner: chapterColor === "cover" ? player.ringInner : Qt.darker(modeColor(chapterColor), 2.4)
    property color progressAccent: modeColor(progressColor, player.ringOuter)
    Behavior on progressAccent { enabled: root.animate; ColorAnimation { duration: 500 } }
    Behavior on ringOuter { enabled: root.animate; ColorAnimation { duration: 500 } }
    Behavior on ringInner { enabled: root.animate; ColorAnimation { duration: 500 } }
    readonly property bool swapRunning: animate && discSwap.running
    function resetSwap() { swapOffset = 0; outgoingOffset = 0; outgoingOpacity = 0; incomingOpacity = 1; lidOpen = 0; packageOpacity = 0 }
    function presentDisc() {
        presentation.present(player.artwork, player.count ? player.albumKey : "", animate && swapAnimation && visible && windowState.exposed)
    }
    onAnimateChanged: if (!animate) { discSwap.stop(); resetSwap(); presentation.releaseOutgoing() }
    Connections {
        target: root.player
        function onTrackChanged() { scrubber.cancelScrub(); Qt.callLater(root.presentDisc) }
        function onArtworkChanged() { Qt.callLater(root.presentDisc) }
    }

    // Record spin (33⅓ rpm) and the wave of the progress ring, on Qt's frame clock
    property real spinAngle: 0
    property real spinSpeed: 0
    property real wavePhase: 0
    FrameAnimation {
        running: root.animate && root.visible && windowState.exposed && (root.player.playing || root.spinSpeed > .02)
        onTriggered: {
            const dt = Math.min(.05, frameTime)
            root.spinSpeed += ((root.player.playing ? 200 : 0) - root.spinSpeed) * Math.min(1, dt * 2.2)
            root.spinAngle = (root.spinAngle + root.spinSpeed * dt) % 360
            if (root.player.playing) {
                root.wavePhase = (root.wavePhase + dt * 2.8) % (2 * Math.PI)
                root.livePosition = root.player.currentPosition()
            }
        }
    }

    PlayerBody {
        x: 45; y: 74; width: 440; height: 440
        surface: root.surface
    }
    // The album sleeve, shown while records are swapped
    PlayerBody {
        x: 45; y: 74; width: 440; height: 440
        visible: root.packageOpacity > 0
        opacity: root.packageOpacity
        transformOrigin: Item.TopLeft
        surface: root.surface; artwork: presentation.artwork; part: 2
        scale: .9 + .1 * root.packageOpacity
    }
    Item {
        id: platter
        x: 45 + 220 * (1 - root.mediumScale); y: 74 + 220 * (1 - root.mediumScale)
        width: 440; height: 440; clip: true
        scale: root.mediumScale
        transformOrigin: Item.TopLeft
        // Shadow is circular, so the player keeps its silhouette on any wallpaper.
        Repeater {
            model: 7
            Rectangle {
                required property int index
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 9
                width: 410 + index * 3; height: width; radius: width / 2
                color: "transparent"; border.width: 8; border.color: "#05000000"
            }
        }
        Loader {
            anchors.centerIn: parent; width: 410; height: 410
            active: root.outgoingOpacity > 0
            visible: active
            opacity: root.outgoingOpacity
            transform: Translate { x: root.outgoingOffset }
            sourceComponent: Item {
                Disc { anchors.fill: parent; artwork: presentation.outgoing; rotation: root.outgoingAngle; tintFromArt: root.recordColor === "cover"; tint: root.recordColor === "system" ? root.accent : "transparent" }
                // The previous video's chapters leave with its record
                ChapterRings {
                    anchors.fill: parent
                    visible: chapters.length > 1
                    chapters: root.outgoingRings.chapters || []
                    duration: root.outgoingRings.duration || 0
                    at: (root.outgoingRings.position || 0) + 300
                    // The previous video's cover colours, or the current mode's colours
                    outer: root.chapterColor === "cover" ? root.outgoingRings.outer || "grey" : root.ringOuter
                    inner: root.chapterColor === "cover" ? root.outgoingRings.inner || "grey" : root.ringInner
                    needleStart: root.needleStart; needleRange: root.needleRange
                }
                Disc { anchors.fill: parent; overlay: true }
            }
        }
        Item {
            anchors.centerIn: parent; width: 410; height: 410
            opacity: root.incomingOpacity
            transform: Translate { x: root.swapOffset }
            enabled: !root.swapRunning
            Disc { anchors.fill: parent; artwork: presentation.artwork; rotation: root.spinAngle; tintFromArt: root.recordColor === "cover"; tint: root.recordColor === "system" ? root.accent : "transparent" }
            // YouTube chapters below the lighting overlay, so the record's sheen passes over them
            // like over the label
            Loader {
                id: chapterRings
                anchors.fill: parent
                active: root.chapterFractions.length > 1
                // Chapters that are already known come with the record; ones that arrive a moment
                // after the video changed fade in
                opacity: 0
                Component.onCompleted: opacity = active ? 1 : 0
                onActiveChanged: if (active && root.animate) ringsFade.restart(); else { ringsFade.stop(); opacity = active ? 1 : 0 }
                NumberAnimation { id: ringsFade; target: chapterRings; property: "opacity"; from: 0; to: 1; duration: 600; easing.type: Easing.InOutQuad }
                sourceComponent: ChapterRings {
                    chapters: root.player.chapters
                    duration: root.player.duration
                    // While the needle is dragged the chapter under it is lit, otherwise the playing one.
                    // recordVisualProgress also holds the drop point until the player reports the new
                    // position, so a drop doesn't flash back to the old chapter; 300 ms of slack
                    // for players that land a little before the chapter start.
                    at: root.recordVisualProgress * duration + (tonearm.previewing ? 0 : 300)
                    fadeTime: tonearm.previewing ? 200 : 600
                    animate: root.animate
                    outer: root.ringOuter; inner: root.ringInner
                    needleStart: root.needleStart; needleRange: root.needleRange
                }
            }
            Disc { anchors.fill: parent; overlay: true }
        }
        Connections {
            target: presentation
            function onSwapRequested() {
                // The presenter has already installed the new outgoing image.
                // Stop the old sequence without releasing that new image.
                discSwap.stop(); root.resetSwap()
                if (!root.animate || !root.visible || !windowState.exposed) { presentation.releaseOutgoing(); return }
                scrubber.cancelScrub(); root.outgoingAngle = root.spinAngle; root.outgoingRings = root.player.previousRings
                root.swapOffset = 440; root.outgoingOffset = 0; root.outgoingOpacity = 1; root.incomingOpacity = 0
                root.packageOpacity = 1
                discSwap.start()
            }
        }
        SequentialAnimation {
            id: discSwap
            NumberAnimation { target: root; property: "lidOpen"; to: 1; duration: Style.exit }
            ParallelAnimation {
                NumberAnimation { target: root; property: "outgoingOffset"; to: -440; duration: Style.enter; easing.type: Easing.BezierSpline; easing.bezierCurve: Style.exitCurve }
                NumberAnimation { target: root; property: "outgoingOpacity"; to: 0; duration: Style.enter }
            }
            ParallelAnimation {
                NumberAnimation { target: root; property: "swapOffset"; to: 0; duration: Style.hero; easing.type: Easing.BezierSpline; easing.bezierCurve: Style.enterCurve }
                NumberAnimation { target: root; property: "incomingOpacity"; to: 1; duration: Style.enter }
                NumberAnimation { target: root; property: "packageOpacity"; to: 0; duration: Style.hero }
            }
            NumberAnimation { target: root; property: "lidOpen"; to: 0; duration: Style.exit }
            onFinished: { root.resetSwap(); presentation.releaseOutgoing() }
        }
        ProgressRing {
            anchors.fill: parent
            visible: root.player.count > 0
            progress: root.ringProgress
            phase: root.wavePhase
            amplitude: root.player.playing ? 2.8 : 0
            accent: root.progressAccent
            Behavior on amplitude { NumberAnimation { duration: Style.enter } }
        }
        // The rim: drag along it to seek (Shift for precision); the wheel over the record sets the volume
        MouseArea {
            id: scrubber
            preventStealing: true
            anchors.fill: parent
            hoverEnabled: true
            property bool scrubbing: false
            property real previewFraction: 0
            property real lastFraction: 0
            readonly property bool canSeek: !root.swapRunning && root.player.duration > 0 && root.player.canSeek
            readonly property bool showPreview: canSeek && (scrubbing || (containsMouse && onRim(mouseX,mouseY)))
            cursorShape: canSeek && onRim(mouseX,mouseY) ? Qt.PointingHandCursor : Qt.ArrowCursor
            function onRim(x,y) { const r=Math.hypot(x-220,y-220); return r > 205 && r < 224 }
            function updatePreview(x,y) {
                let a=Math.atan2(y-220,x-220)+Math.PI/2
                if (a < 0) a += 2*Math.PI
                previewFraction = a/(2*Math.PI)
            }
            function dragPreview(x,y,fine) {
                const previous=previewFraction
                updatePreview(x,y)
                const pointer=previewFraction
                let delta=pointer-lastFraction
                if(delta>.5)delta-=1;else if(delta < -.5)delta+=1
                previewFraction=Math.max(0,Math.min(1,previous+delta*(fine? 0.1:1)))
                lastFraction=pointer
            }
            function cancelScrub() { scrubbing = false }
            onPressed: mouse => {
                if (!onRim(mouse.x,mouse.y) || !canSeek) { mouse.accepted=false; return }
                updatePreview(mouse.x,mouse.y); lastFraction=previewFraction
                if ((mouse.modifiers & Qt.ShiftModifier) || root.shiftHeld) previewFraction=root.progress
                scrubbing=true
            }
            onPositionChanged: mouse => { if(scrubbing)dragPreview(mouse.x,mouse.y,(mouse.modifiers & Qt.ShiftModifier) || root.shiftHeld);else if(onRim(mouse.x,mouse.y))updatePreview(mouse.x,mouse.y) }
            onReleased: {
                if (scrubbing && canSeek) root.player.seek(root.player.duration * previewFraction)
                scrubbing=false
            }
            onCanceled: cancelScrub()
            onWheel: wheel => { root.player.volume = Math.max(0, Math.min(1, root.player.volume + wheel.angleDelta.y/2400)); wheel.accepted=true }
        }
    }
    // The arm, above the record and the seek ring, in platter coordinates but not clipped by it:
    // at the rest the headshell's shadow reaches past the platter's right edge
    Item {
        x: platter.x; y: platter.y; width: 470; height: 440
        scale: platter.scale; transformOrigin: Item.TopLeft
        Tonearm { id: tonearm; anchors.fill: parent; app: root }
    }
    // The lid strip and spindle above the record; it lifts while records are swapped
    PlayerBody {
        x: 45; y: 74; width: 440; height: 440
        surface: root.surface; part: 1
        opacity: 1 - .75 * root.lidOpen
        transform: Scale { origin.x: 220; origin.y: 26; yScale: 1 - .35 * root.lidOpen }
    }
    // Time (and chapter) while seeking on the rim, pinned to the record's left edge at the same height
    Rectangle {
        visible: scrubber.showPreview
        z: 10
        readonly property real angle: scrubber.previewFraction * 2 * Math.PI
        readonly property point location: platter.mapToItem(root.contentItem, 220 + Math.sin(angle)*170, 220 - Math.cos(angle)*170)
        readonly property real leftEdge: { const dy = - Math.cos(angle)*170; return platter.mapToItem(root.contentItem, 220 - Math.sqrt(Math.max(0, 196*196 - dy*dy)) + 10, 0).x }
        x: leftEdge; y: location.y - height/2
        width: Math.max(58, rimText.implicitWidth + 24); height: 26; radius: 13
        color: root.surface
        Text {
            id: rimText
            anchors.centerIn: parent
            text: { const ms = root.player.duration * scrubber.previewFraction, chapter = root.chapterAt(ms); return root.time(ms) + (chapter ? " · " + chapter : "") }
            font.pixelSize: Style.caption; color: root.progressAccent
        }
    }
}
