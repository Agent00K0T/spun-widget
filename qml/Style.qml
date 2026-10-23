pragma Singleton
import QtQuick
// Sizes, durations and curves shared by the scene (from Spun's SpunStyle.qml).
// Durations follow Noctalia's animation speed and are 0 with animations off.
QtObject {
    readonly property int caption: 12
    readonly property int gap: 8
    readonly property int textGap: 12
    readonly property real rowRadius: 12 * theme.radius
    readonly property bool motion: theme.motionScale > 0
    readonly property int feedback: Math.round(150 * theme.motionScale)
    readonly property int enter: Math.round(200 * theme.motionScale)
    readonly property int exit: Math.round(100 * theme.motionScale)
    readonly property int hero: Math.round(350 * theme.motionScale)
    // Material's emphasized entrance/exit easing, in Qt's cubic format
    readonly property var effectsCurve: [0.31, 0.94, 0.34, 1, 1, 1]
    readonly property var standardCurve: [0.2, 0, 0, 1, 1, 1]
    readonly property var enterCurve: [0.05, 0.7, 0.1, 1, 1, 1]
    readonly property var exitCurve: [0.3, 0, 0.8, 0.15, 1, 1]
    readonly property color popup: Qt.tint(theme.colors.card, Qt.alpha(theme.colors.text, .035))
}
