import QtQuick
import QtQuick.Controls

// A themed, non-interactive tooltip (Spun's SpunToolTip.qml).
ToolTip {
    id: bubble
    popupType: Popup.Item
    focus: false
    modal: false
    dim: false
    enabled: false
    closePolicy: Popup.NoAutoClose
    timeout: -1
    margins: 8
    horizontalPadding: Style.textGap
    verticalPadding: Style.gap
    implicitWidth: Math.min(260, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    font.pixelSize: Style.caption
    font.weight: Font.Normal
    contentItem: Text {
        text: bubble.text
        font: bubble.font
        textFormat: Text.PlainText
        color: theme.colors.text
        wrapMode: Text.WordWrap
        lineHeightMode: Text.FixedHeight; lineHeight: 16
    }
    background: Rectangle {
        color: Style.popup
        radius: Style.rowRadius
        border.width: 0
    }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Style.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: Style.effectsCurve } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Style.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: Style.effectsCurve } }
}
