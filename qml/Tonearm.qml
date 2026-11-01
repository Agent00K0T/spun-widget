import QtQuick
import QtQuick.Window

// The tonearm (Spun's Tonearm.qml in its widget layout). Drag the needle onto the grooves to
// seek, click it to pause or resume, drop it on the label for the next track or off the record
// to pause where it was left.
Item {
    id: arm
    required property var app
    // Rendered multisampled, so thin parts like the counterweight knurling keep even spacing
    // while the arm turns (aliased, the hairlines snap to pixel rows and drift). The texture
    // matches the on-screen size (the platter is scaled down), so it is not resampled.
    layer.enabled: true; layer.samples: 8; layer.smooth: true
    layer.textureSize: Qt.size(Math.round(width / screenPixel), Math.round(height / screenPixel))
    // One screen pixel in the arm's own units (the platter and the window are scaled)
    readonly property real screenPixel: 1 / (app.mediumScale * app.uiScale * Screen.devicePixelRatio)
    readonly property bool motion: app.animate && visible && app.visible && windowState.exposed
    readonly property bool engaged: visible && app.player.count > 0 && ((app.player.playing && !app.swapRunning) || landing || dragging || app.otherSeekPreview)
    readonly property bool canSeek: visible && app.player.duration > 0 && app.player.canSeek
    property bool dragging: false
    // The arm follows the pointer directly (no animation). Cleared before dragging on release, so
    // the animation is already on when the arm's target changes (binding order is not defined)
    property bool following: false
    // Pivot and arm length in platter units; the music runs from startAngle to startAngle + angleRange
    readonly property real pivotX: 378
    readonly property real pivotY: 100
    readonly property real armLength: 197
    readonly property real startAngle: app.needleStart
    readonly property real angleRange: app.needleRange
    readonly property real restAngle: app.needleRest
    property real dragAngle: restAngle
    property real rawAngle: restAngle
    property real lastPointerAngle: 0
    property real pointerDistance: 210
    property string dragTrack: ""
    property bool wasPlaying: false
    property bool landing: false
    property real landingProgress: 0
    // Shift drag: 10x slower, and the needle sticks to a chapter start when it comes within
    // 0.35° of it, letting go again 0.9° further on (-1 = not stuck)
    property real stuckFraction: -1
    property real pressAngle: 0
    // The needle moved by at least 1° since the press: a drag, not a click. Only then the ring
    // previews the needle position (at the rest that would be 0 and the ring would vanish)
    property bool moved: false
    // No track at all: the needle can still be played with and swings back to the rest on release
    readonly property bool idle: app.player.count === 0
    readonly property bool previewing: dragging && moved && !idle
    // Dragged past the last groove onto the label means "next track"
    readonly property bool overLabel: dragging && rawAngle > startAngle + angleRange + 4
    // On the lead-in (from the record's edge to the first groove) the track starts from the
    // beginning; past the edge the needle is off the record
    readonly property bool overLeadIn: dragging && rawAngle >= app.needleEdge && rawAngle < startAngle
    readonly property bool offRecord: dragging && rawAngle < app.needleEdge
    readonly property real rawProgress: Math.max(0, Math.min(1, (rawAngle - startAngle) / angleRange))
    readonly property real previewProgress: stuckFraction >= 0 ? stuckFraction : rawProgress
    // previous: rawAngle before this pointer event. Pointer events can arrive in bursts, so
    // crossing a chapter start also sticks, unless the needle flew more than 0.9° past it.
    function updateMagnet(shift, previous) {
        if (!shift || !app.chapterFractions.length) { stuckFraction = -1; return }
        if (stuckFraction >= 0 && Math.abs(rawAngle - (startAngle + angleRange * stuckFraction)) > .9) stuckFraction = -1
        if (stuckFraction >= 0) return
        const nearest = app.nearestChapterFraction(rawProgress)
        const chapterAngle = startAngle + angleRange * nearest, distance = Math.abs(rawAngle - chapterAngle)
        const crossed = (previous - chapterAngle) * (rawAngle - chapterAngle) <= 0
        if (distance < .35 || (crossed && distance <= .9)) stuckFraction = nearest
    }
    // When the widget appears while a track already plays, the arm starts at the rest and
    // swings onto the record once the window is on screen (animations only run then)
    property bool introDone: false
    Timer { interval: 500; running: !arm.introDone && arm.motion; onTriggered: arm.introDone = true }
    readonly property bool onRecord: engaged && introDone
    // How far the arm is lowered (1 on the record, 0 lifted): its shadow moves out as it lifts
    property real lowered: dragging ? .12 : onRecord ? 1 : 0
    Behavior on lowered {
        enabled: arm.motion && !arm.following
        NumberAnimation { id: liftMotion; duration: Style.hero; easing.type: Easing.BezierSpline; easing.bezierCurve: Style.standardCurve }
    }
    // A needle dropped off the record stays where it was left (playback paused)
    // until playback starts again; NaN = not parked
    property real parkedAngle: NaN
    onEngagedChanged: if (engaged) parkedAngle = NaN
    readonly property real groove: Math.max(0, Math.min(1, app.recordVisualProgress))
    readonly property real targetAngle: dragging ? dragAngle : onRecord ? startAngle + angleRange * groove : !isNaN(parkedAngle) ? parkedAngle : restAngle
    // Swings take a fixed time (the outer and inner grooves are reached equally fast), so the
    // arm is aimed imperatively: while a swing runs, the slowly moving groove does not restart it
    property real armAngle: restAngle
    property real aimedAngle: restAngle
    function aim() {
        if (targetAngle === aimedAngle || (swing.running && Math.abs(targetAngle - aimedAngle) < .5)) return
        aimedAngle = targetAngle; armAngle = targetAngle
    }
    onTargetAngleChanged: aim()
    Component.onCompleted: { aimedAngle = targetAngle; armAngle = targetAngle }
    Behavior on armAngle {
        enabled: arm.motion && !arm.following && !arm.app.otherSeekPreview
        NumberAnimation { id: swing; duration: 900; easing.type: Easing.InOutSine; onRunningChanged: if (!running) arm.aim() }
    }
    onMotionChanged: if (!motion) { liftMotion.complete(); swing.complete() }
    function cancelDrag(resume) {
        if (!dragging) return
        following = false; app.easeRing()
        dragging = false; landing = false
        if (resume && wasPlaying && dragTrack === app.player.trackKey) app.player.play()
    }
    function dropNeedle() {
        if (!dragging) return
        following = false
        if (idle) { dragging = false; stuckFraction = -1; parkedAngle = NaN; return }
        app.easeRing()
        const valid = canSeek && dragTrack === app.player.trackKey && rawAngle >= app.needleEdge && rawAngle <= startAngle+angleRange+2 && pointerDistance > armLength*.6 && pointerDistance < armLength*1.5
        const position = previewProgress
        if (!moved) {
            // A click on the needle toggles playback without seeking. Playing: it stays paused
            // (pressing already paused it) and the arm lifts back to the rest. Paused: the arm
            // lowers onto the stopped position right away and playback resumes there.
            dragging = false; stuckFraction = -1; parkedAngle = NaN
            if (!wasPlaying) {
                landingProgress = app.progress; landing = true; landingTimeout.restart()
                app.player.play()
            }
            return
        }
        if (overLabel) {
            dragging = false
            if (app.player.canNext) { app.player.next(); app.player.play() }
            return
        }
        if (!valid) { if (dragAngle < startAngle) parkedAngle = dragAngle; dragging = false; return }
        landingProgress = position; landing = true; landingTimeout.restart()
        dragging = false
        app.player.seek(Math.min(app.player.duration - 1, app.player.duration * position))
        app.player.play()
    }
    function confirmLanding() {
        if (landing && app.player.playing && Math.abs(app.progress - landingProgress) < .025) {
            landing = false; landingTimeout.stop()
        }
    }
    onVisibleChanged: if (!visible) cancelDrag(true)
    // Another track: a drag or landing that belonged to the previous one is void
    readonly property string trackKey: app.player.trackKey
    onTrackKeyChanged: { cancelDrag(false); landing = false }
    onCanSeekChanged: if (!canSeek) cancelDrag(false)
    Timer { id: landingTimeout; interval: 1200; onTriggered: arm.landing = false }
    Connections {
        target: arm.app
        function onOtherSeekPreviewChanged() {
            if (arm.app.otherSeekPreview) { arm.landing=false; landingTimeout.stop() }
        }
        function onVisibleChanged() { if(!arm.app.visible)arm.cancelDrag(true) }
    }
    Connections {
        target: arm.app.player
        function onPositionChanged() { arm.confirmLanding() }
        function onPlayingChanged() { arm.confirmLanding() }
    }
    readonly property color gold: Qt.tint("#c6a25a", Qt.alpha(app.armAccent, .12))
    readonly property color glint: Qt.lighter(gold, 1.5)
    readonly property color shade: Qt.darker(gold, 1.6)
    Bubble {
        // At the record's left edge (same height), growing to the right
        parent: arm; x: 100; y: 60
        // The time label waits until the needle actually moves, so a click doesn't flash it;
        // the hover hint shows only after the pointer rests on the needle a while
        visible: arm.canSeek && arm.app.visible && (arm.previewing || (!arm.dragging && !needleHit.hintDismissed && needleHit.hoveringNeedle))
        delay: arm.dragging ? 0 : 1500
        text: {
            if (arm.overLabel) return arm.app.player.canNext ? qsTr("Next track") : qsTr("No next track")
            if (arm.overLeadIn) return qsTr("From the start")
            if (arm.offRecord) return qsTr("Pause")
            if (arm.dragging) {
                const ms = arm.previewProgress * arm.app.player.duration, chapter = arm.app.chapterAt(ms)
                return arm.app.time(ms) + (chapter ? " · " + chapter : "")
            }
            if (arm.app.chapterFractions.length) return qsTr("Drag the needle to seek · Shift for precision, snapping to chapters")
            return qsTr("Drag the needle to seek · Shift for precision")
        }
    }
    // The arm rest, turned about the pivot to where the arm lies
    Rectangle {
        x: 377; y: 105; width: 14; height: 35; radius: 5
        color: arm.app.surface; border.width: 1; border.color: arm.shade
        transform: Rotation { origin.x: 1; origin.y: -5; angle: arm.restAngle }
    }
    // The pivot rests at the record's edge; the stylus stays outside the label.
    Item {
        x: arm.pivotX; y: arm.pivotY
        Rectangle { x: -21; y: -17; width: 42; height: 42; radius: 21; color: "#80000000" }
        Rectangle {
            x: -19; y: -19; width: 38; height: 38; radius: 19
            gradient: Gradient { GradientStop { position: 0; color: "#a0a5a5" } GradientStop { position: .12; color: "#555c5e" } GradientStop { position: .6; color: "#252a2e" } GradientStop { position: 1; color: "#101619" } }
            border.width: .8; border.color: "#747b7b"
            Rectangle { anchors.centerIn: parent; width: 31; height: 31; radius: 15.5; color: "#252b2d"; border.width: 1; border.color: "#121718" }
        }
        Rectangle {
            x: -13; y: -13; width: 26; height: 26; radius: 13
            color: arm.app.surface; border.width: 1; border.color: arm.shade
        }
        Item {
            rotation: arm.armAngle
            transformOrigin: Item.TopLeft
            Rectangle { x: -3; y: -47; width: 6; height: 39; radius: 2; color: arm.shade }
            Rectangle {
                x: -15; y: -45; width: 30; height: 27; radius: 5
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#242b2e" }
                    GradientStop { position: .16; color: "#737b7d" }
                    GradientStop { position: .34; color: "#c0c6c6" }
                    GradientStop { position: .65; color: "#6e7678" }
                    GradientStop { position: 1; color: "#1c2326" }
                }
                border.width: .7; border.color: "#7a8486"
                Repeater {
                    // Knurling: the platter is scaled down, so lines thinner than a screen pixel would
                    // fade and bunch up as the arm turns. They are exactly one screen pixel thick and
                    // three apart (as many as fit above the ring).
                    model: Math.floor((22 - 3) / (3 * arm.screenPixel)) + 1
                    Rectangle { required property int index; x: 2; y: 3 + index * 3 * arm.screenPixel; width: 26; height: arm.screenPixel; color: "#70000000" }
                }
                Rectangle { x: 1; y: 23; width: 28; height: 3; radius: 1; color: "#262d30" }
            }
            // A displaced hard shadow makes the lift legible without a blur pass.
            Rectangle { x: 1 + (1 - arm.lowered) * 2; y: 5; width: 7; height: 187; radius: 3.5; color: "#48000000" }
            Rectangle { x: (1 - arm.lowered) * 2; y: 188; width: 14; height: 24; radius: 4; color: "#48000000" }
            Rectangle {
                x: -7; y: -14; width: 14; height: 22; radius: 5
                gradient: Gradient {
                    GradientStop { position: 0; color: arm.shade }
                    GradientStop { position: .4; color: arm.glint }
                    GradientStop { position: 1; color: arm.gold }
                }
            }
            Rectangle {
                x: -3; y: 0; width: 6; height: 187; radius: 3
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: arm.shade }
                    GradientStop { position: .36; color: arm.glint }
                    GradientStop { position: .65; color: arm.gold }
                    GradientStop { position: 1; color: arm.shade }
                }
            }
            Rectangle { x: -4; y: 178; width: 8; height: 10; radius: 2; color: arm.gold }
            Rectangle {
                x: -7; y: 185; width: 14; height: 24; radius: 4
                color: Qt.tint(arm.app.surface, Qt.alpha(arm.gold, .15))
                border.width: 1; border.color: arm.gold
                Rectangle { x: 4; y: 5; width: 6; height: 2; radius: 1; color: arm.gold }
                Rectangle { x: 4; y: 10; width: 6; height: 2; radius: 1; color: arm.gold }
                Rectangle { x: 2; y: 18; width: 10; height: 5; radius: 1; color: "#171b1d" }
                Rectangle { x: 6; y: 23; width: 1.2; height: 5; radius: .6; color: "#d5dcde" }
                Rectangle { x: 5.5; y: 27; width: 2; height: 1.5; radius: .5; color: "#d6c5a8" }
                Rectangle { x: 12; y: 4; width: 9; height: 2; radius: 1; rotation: -20; color: arm.gold }
                Repeater {
                    model: 2
                    Rectangle { required property int index; x: 2+index*8; y: 2; width: 2; height: 2; radius: 1; color: "#d1d4ca" }
                }
            }
        }
        Rectangle { x: -7; y: -7; width: 14; height: 14; radius: 7; color: arm.gold; border.width: 1; border.color: arm.glint }
        Rectangle { x: -3; y: -1; width: 6; height: 2; radius: 1; rotation: -35; color: arm.shade }
    }
    HoverHandler { id: needleHover; blocking: false; onHoveredChanged: if (!hovered) needleHit.hintDismissed = false }
    // Pointer coordinates must stay fixed while the arm rotates beneath them.
    MouseArea {
        id: needleHit
        anchors.fill: parent
        // Always draggable: before the length is known a click resumes playback, without a track
        // the needle is just moved about and returns to the rest
        enabled: arm.visible
        hoverEnabled: true; preventStealing: true
        acceptedButtons: Qt.LeftButton
        property bool hintDismissed: false
        readonly property bool hoveringNeedle: needleHover.hovered && Math.hypot(needleHover.point.position.x-tip.x, needleHover.point.position.y-tip.y) <= 27
        onHoveringNeedleChanged: if (!hoveringNeedle) hintDismissed = false
        readonly property point tip: {
            const angle = arm.armAngle * Math.PI / 180
            return Qt.point(arm.pivotX - arm.armLength * Math.sin(angle), arm.pivotY + arm.armLength * Math.cos(angle))
        }
        containmentMask: QtObject {
            function contains(point: point): bool { return Math.hypot(point.x - needleHit.tip.x, point.y - needleHit.tip.y) <= 27 }
        }
        cursorShape: arm.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        onPressed: mouse => {
            if (Math.hypot(mouse.x - tip.x, mouse.y - tip.y) > 27) { mouse.accepted = false; return }
            arm.dragAngle = arm.armAngle; arm.rawAngle = arm.armAngle; arm.pressAngle = arm.armAngle; arm.stuckFraction = -1; arm.moved = false
            arm.lastPointerAngle = Math.atan2(arm.pivotX - mouse.x, mouse.y - arm.pivotY) * 180 / Math.PI
            arm.dragTrack = arm.app.player.trackKey; arm.wasPlaying = arm.app.player.playing
            arm.pointerDistance = Math.hypot(mouse.x - arm.pivotX, mouse.y - arm.pivotY)
            arm.following = true; arm.dragging = true; arm.landing = false; landingTimeout.stop()
            if (arm.wasPlaying) arm.app.player.pause()
        }
        onPositionChanged: mouse => {
            if (!pressed || !arm.dragging) return
            const dx = mouse.x - arm.pivotX, dy = mouse.y - arm.pivotY
            arm.pointerDistance = Math.hypot(dx, dy)
            const pointer = Math.atan2(-dx, dy) * 180 / Math.PI
            let delta = pointer - arm.lastPointerAngle
            if (delta > 180) delta -= 360; else if (delta < -180) delta += 360
            const shift = (mouse.modifiers & Qt.ShiftModifier) !== 0 || arm.app.shiftHeld
            const previous = arm.rawAngle
            arm.rawAngle += delta * (shift ? 0.1 : 1)
            arm.lastPointerAngle = pointer
            if (Math.abs(arm.rawAngle - arm.pressAngle) >= 1) arm.moved = true
            arm.updateMagnet(shift, previous)
            // The rest is the outer stop (further out the headshell leaves the clipped platter)
            arm.dragAngle = arm.stuckFraction >= 0
                ? arm.startAngle + arm.angleRange * arm.stuckFraction
                : Math.max(arm.restAngle, Math.min(arm.startAngle + arm.angleRange + 12, arm.rawAngle))
        }
        onReleased: { hintDismissed = true; arm.dropNeedle() }
        onCanceled: { hintDismissed = true; arm.cancelDrag(true) }
    }
}
