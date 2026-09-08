// SPDX-FileCopyrightText: 2026 song
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dock-lyrics: a dock plugin that wakes up when music is playing
// and shows the current lyric line as a marquee on the taskbar.

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1 as LP

import org.deepin.ds 1.0
import org.deepin.dtk 1.0 as D
import org.deepin.ds.dock 1.0

AppletItem {
    id: root

    /* ---------- dock integration ---------- */
    property int dockOrder: 21
    property bool shouldVisible: Applet.playing
    // Panel.position: 0 bottom / 1 right / 2 top / 3 left (odd == vertical dock)
    property bool verticalDock: Panel.position % 2 === 1

    /* ---------- colour themes ---------- */
    property D.Palette iconTextPalette: DockPalette.iconTextPalette
    property D.Palette textPalette: DockPalette.iconTextPalette
    readonly property int customPaletteIndex: Applet.colorThemeNames.length - 1   // 11 == 自定义配色
    readonly property bool vividTheme: Applet.colorTheme > 0 && Applet.colorTheme < customPaletteIndex
    readonly property bool customPalette: Applet.colorTheme === customPaletteIndex
    readonly property bool customFontSet: customPalette && Applet.customTextColor.length > 0
    readonly property bool customBgSet: customPalette && Applet.customBgColor.length > 0
    readonly property color themeColorValue: Applet.colorThemeColors.length > 0
        ? Applet.colorThemeColors[Math.min(Applet.colorTheme, Applet.colorThemeColors.length - 1)]
        : "#FFFFFF"
    // preset dark themes carry their own pill background (empty == keep system behaviour)
    readonly property string themeBgValue: Applet.colorThemeBgColors.length > 0
        ? Applet.colorThemeBgColors[Math.min(Applet.colorTheme, Applet.colorThemeBgColors.length - 1)]
        : ""
    readonly property bool presetDarkBg: themeBgValue.length > 0
    readonly property bool showPill: presetDarkBg || customBgSet
    readonly property color pillColor: presetDarkBg ? themeBgValue
                                                    : (customBgSet ? Applet.customBgColor : "transparent")
    // lyric / eq colour resolution:
    //  - 自定义配色 with its own font colour wins; otherwise a dark pill gets soft white text
    //  - preset vivid themes use their accent; 0 == follow the system palette
    readonly property color lyricColor: customFontSet ? Applet.customTextColor
        : (vividTheme ? themeColorValue
           : (customBgSet ? "#E6EBF7" : D.ColorSelector.textPalette))
    readonly property color eqColor: customFontSet ? Applet.customTextColor
        : (vividTheme ? themeColorValue
           : (customBgSet ? "#E6EBF7" : D.ColorSelector.iconTextPalette))

    /* ---------- metrics ---------- */
    readonly property int dockSize: Panel.rootObject.dockSize
    readonly property real textPixel: Math.max(11, Math.round(dockSize * 0.34))
    readonly property int textAreaWidth: Math.max(90, Math.round(dockSize * 3.6))
    readonly property int eqWidth: Math.round(dockSize * 1.1)
    readonly property int contentWidth: verticalDock ? dockSize : (eqWidth + textAreaWidth + 14)

    implicitWidth: contentWidth
    implicitHeight: dockSize

    /* ---------- presentation text ---------- */
    readonly property string songInfo: {
        var t = Applet.title
        var a = Applet.artist
        return a.length > 0 ? t + " - " + a : t
    }

    property string marqueeText: ""
    property int plainIndex: 0

    function refreshDisplayText() {
        if (Applet.synced && Applet.line.length > 0) {
            marqueeText = Applet.line
            return
        }
        if (Applet.hasLyrics && !Applet.synced && Applet.lyricLines.length > 0) {
            marqueeText = Applet.lyricLines[plainIndex % Applet.lyricLines.length]
            return
        }
        if (Applet.loadingLyrics) {
            marqueeText = songInfo.length > 0 ? songInfo : qsTr("Loading lyrics…")
            return
        }
        // no usable lyrics: show the song title as a marquee
        marqueeText = songInfo.length > 0 ? songInfo : Applet.stateText
    }

    Connections {
        target: Applet
        onPlayingChanged: refreshDisplayText()
        onLineChanged: refreshDisplayText()
        onLyricsChanged: refreshDisplayText()
        onSongChanged: refreshDisplayText()
    }

    // slow page-flip for untimed (plain) lyrics
    Timer {
        interval: 2600
        repeat: true
        running: Applet.hasLyrics && !Applet.synced && Applet.lyricLines.length > 1
        onTriggered: {
            plainIndex = (plainIndex + 1) % Applet.lyricLines.length
            refreshDisplayText()
        }
    }

    Component.onCompleted: {
        console.warn("[dock-lyrics-qml] onCompleted playing=" + Applet.playing
                    + " title=" + Applet.title + " artist=" + Applet.artist
                    + " hasLyrics=" + Applet.hasLyrics + " synced=" + Applet.synced
                    + " state=" + Applet.stateText)
        refreshDisplayText()
    }

    /* ---------- lyric-source label for the context menu ---------- */
    readonly property string sourceLabel: {
        if (Applet.loadingLyrics)
            return qsTr("正在获取歌词…")
        switch (Applet.lyricSource) {
        case "local":   return qsTr("歌词来源：本地文件")
        case "netease": return qsTr("歌词来源：网易云音乐")
        case "lrclib":  return qsTr("歌词来源：LRCLIB")
        }
        if (Applet.hasLyrics)
            return qsTr("歌词来源：未知")
        return qsTr("暂无歌词")
    }

    /* ---------- tooltip ---------- */
    readonly property string tooltipText: {
        var info = songInfo.length > 0 ? songInfo : Applet.stateText
        var hint = "\n" + qsTr("Click to play / pause")
        if (Applet.hasLyrics)
            return info + "\n" + marqueeText + hint
        if (Applet.stateText.length > 0)
            return info + "\n" + Applet.stateText + hint
        return info + hint
    }

    PanelToolTip {
        id: toolTip
        text: root.tooltipText
        toolTipX: DockPanelPositioner.x
        toolTipY: DockPanelPositioner.y
    }

    /* ---------- interaction ---------- */
    TapHandler {
        id: tapHandler
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.WithinBounds
        onTapped: Applet.playPause()
    }

    // right click -> context menu
    TapHandler {
        id: contextTapHandler
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.WithinBounds
        onTapped: {
            toolTip.close()
            MenuHelper.openMenu(contextMenuLoader.item)
        }
    }

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: {
            if (hovered) {
                var point = root.mapToItem(null, root.width / 2, root.height / 2)
                toolTip.DockPanelPositioner.bounding = Qt.rect(point.x, point.y, toolTip.width, toolTip.height)
                toolTip.open()
            } else {
                toolTip.close()
            }
        }
    }

    /* ===================================================
     *  horizontal dock (bottom / top)
     * =================================================== */
    Rectangle {
        id: bg
        anchors.fill: parent
        visible: !root.verticalDock
        radius: Math.round(root.dockSize * 0.16)
        property D.Palette bgPalette: DockPalette.backgroundPalette
        property bool bgHovered: false
        // dark themes / custom pill colours keep the pill always visible; others only while hovered
        color: root.showPill ? root.pillColor
                             : (bgHovered ? D.ColorSelector.bgPalette : "transparent")
        HoverHandler {
            id: pillHover
            onHoveredChanged: bg.bgHovered = hovered
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 5

            // --- left: animated equalizer icon ---
            Item {
                width: root.eqWidth
                height: parent.height
                Item {
                    id: eqBox
                    anchors.centerIn: parent
                    width: 22
                    height: Math.round(root.dockSize * 0.46)
                    property color barColor: root.eqColor

                    Rectangle {
                        id: eq1
                        width: 3; radius: 1.5
                        height: 6
                        color: eqBox.barColor
                        anchors.bottom: parent.bottom; anchors.left: parent.left
                    }
                    Rectangle {
                        id: eq2
                        width: 3; radius: 1.5
                        height: 13
                        color: eqBox.barColor
                        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Rectangle {
                        id: eq3
                        width: 3; radius: 1.5
                        height: 9
                        color: eqBox.barColor
                        anchors.bottom: parent.bottom; anchors.right: parent.right
                    }

                    SequentialAnimation {
                        running: Applet.playing; loops: Animation.Infinite
                        NumberAnimation { target: eq1; property: "height"; from: 6; to: Math.max(8, eqBox.height); duration: 260; easing.type: Easing.InOutSine }
                        NumberAnimation { target: eq1; property: "height"; from: Math.max(8, eqBox.height); to: 6; duration: 260; easing.type: Easing.InOutSine }
                    }
                    SequentialAnimation {
                        running: Applet.playing; loops: Animation.Infinite
                        NumberAnimation { target: eq2; property: "height"; from: eqBox.height; to: 6; duration: 300; easing.type: Easing.InOutSine }
                        NumberAnimation { target: eq2; property: "height"; from: 6; to: eqBox.height; duration: 300; easing.type: Easing.InOutSine }
                    }
                    SequentialAnimation {
                        running: Applet.playing; loops: Animation.Infinite
                        NumberAnimation { target: eq3; property: "height"; from: 7; to: eqBox.height; duration: 340; easing.type: Easing.InOutSine }
                        NumberAnimation { target: eq3; property: "height"; from: eqBox.height; to: 7; duration: 240; easing.type: Easing.InOutSine }
                    }
                }
            }

            // --- right: lyric marquee ---
            Item {
                id: marqueeView
                width: root.textAreaWidth
                height: parent.height
                clip: true

                Text {
                    id: lyricText
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.marqueeText
                    font.pixelSize: root.textPixel
                    color: root.lyricColor

                    property bool tooWide: width > parent.width
                    property real travel: 0

                    onWidthChanged: {
                        if (root.verticalDock) return
                        if (!tooWide) { marqueeSeq.stop(); x = 0; return }
                        x = 0
                        travel = Math.max(1, width - parent.width + 30)
                        marqueeSeq.restart()
                    }
                }

                // endless marquee: pause -> slide left -> pause -> slide back
                SequentialAnimation {
                    id: marqueeSeq
                    running: Applet.playing && lyricText.tooWide && !root.verticalDock
                    loops: Animation.Infinite
                    PauseAnimation { duration: 1600 }
                    NumberAnimation {
                        target: lyricText
                        property: "x"
                        to: -lyricText.travel
                        duration: Math.max(800, lyricText.travel * 40)
                        easing.type: Easing.Linear
                    }
                    PauseAnimation { duration: 1400 }
                    NumberAnimation {
                        target: lyricText
                        property: "x"
                        to: 0
                        duration: 500
                        easing.type: Easing.OutQuad
                    }
                }
            }
        }
    }

    /* ===================================================
     *  vertical dock (left / right): equalizer only
     * =================================================== */
    Item {
        anchors.fill: parent
        visible: root.verticalDock
        Rectangle {
            anchors.fill: parent
            visible: root.showPill
            radius: Math.round(root.dockSize * 0.16)
            color: root.pillColor
        }
        Item {
            id: veq
            anchors.centerIn: parent
            width: 22
            height: Math.round(root.dockSize * 0.46)
            property color barColor: root.eqColor

            Rectangle {
                id: vb1
                width: 3; radius: 1.5; height: 6
                color: veq.barColor
                anchors.bottom: parent.bottom; anchors.left: parent.left
            }
            Rectangle {
                id: vb2
                width: 3; radius: 1.5; height: 13
                color: veq.barColor
                anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
            }
            Rectangle {
                id: vb3
                width: 3; radius: 1.5; height: 9
                color: veq.barColor
                anchors.bottom: parent.bottom; anchors.right: parent.right
            }

            SequentialAnimation {
                running: Applet.playing; loops: Animation.Infinite
                NumberAnimation { target: vb1; property: "height"; from: 6; to: Math.max(8, veq.height); duration: 260; easing.type: Easing.InOutSine }
                NumberAnimation { target: vb1; property: "height"; from: Math.max(8, veq.height); to: 6; duration: 260; easing.type: Easing.InOutSine }
            }
            SequentialAnimation {
                running: Applet.playing; loops: Animation.Infinite
                NumberAnimation { target: vb2; property: "height"; from: veq.height; to: 6; duration: 300; easing.type: Easing.InOutSine }
                NumberAnimation { target: vb2; property: "height"; from: 6; to: veq.height; duration: 300; easing.type: Easing.InOutSine }
            }
            SequentialAnimation {
                running: Applet.playing; loops: Animation.Infinite
                NumberAnimation { target: vb3; property: "height"; from: 7; to: veq.height; duration: 340; easing.type: Easing.InOutSine }
                NumberAnimation { target: vb3; property: "height"; from: veq.height; to: 7; duration: 240; easing.type: Easing.InOutSine }
            }
        }
    }
    /* ---------- context menu ---------- */
    Loader {
        id: contextMenuLoader
        active: true
        sourceComponent: LP.Menu {
            id: ctxMenu

            LP.MenuItem {
                text: Applet.playing ? qsTr("暂停") : qsTr("播放")
                onTriggered: Applet.playPause()
            }
            LP.MenuItem {
                text: qsTr("上一曲")
                onTriggered: Applet.previous()
            }
            LP.MenuItem {
                text: qsTr("下一曲")
                onTriggered: Applet.next()
            }
            LP.MenuSeparator {}

            LP.Menu {
                id: themeMenu
                title: qsTr("颜色主题")
                LP.MenuItemGroup {
                    id: themeGroup
                    items: themeMenu.items
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[0]
                    checkable: true
                    checked: Applet.colorTheme === 0
                    onTriggered: Applet.colorTheme = 0
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[1]
                    checkable: true
                    checked: Applet.colorTheme === 1
                    onTriggered: Applet.colorTheme = 1
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[2]
                    checkable: true
                    checked: Applet.colorTheme === 2
                    onTriggered: Applet.colorTheme = 2
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[3]
                    checkable: true
                    checked: Applet.colorTheme === 3
                    onTriggered: Applet.colorTheme = 3
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[4]
                    checkable: true
                    checked: Applet.colorTheme === 4
                    onTriggered: Applet.colorTheme = 4
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[5]
                    checkable: true
                    checked: Applet.colorTheme === 5
                    onTriggered: Applet.colorTheme = 5
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[6]
                    checkable: true
                    checked: Applet.colorTheme === 6
                    onTriggered: Applet.colorTheme = 6
                }
                LP.MenuSeparator {}
                LP.MenuItem { text: qsTr("深色主题"); enabled: false }
                LP.MenuItem {
                    text: Applet.colorThemeNames[7]
                    checkable: true
                    checked: Applet.colorTheme === 7
                    onTriggered: Applet.colorTheme = 7
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[8]
                    checkable: true
                    checked: Applet.colorTheme === 8
                    onTriggered: Applet.colorTheme = 8
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[9]
                    checkable: true
                    checked: Applet.colorTheme === 9
                    onTriggered: Applet.colorTheme = 9
                }
                LP.MenuItem {
                    text: Applet.colorThemeNames[10]
                    checkable: true
                    checked: Applet.colorTheme === 10
                    onTriggered: Applet.colorTheme = 10
                }
                LP.MenuSeparator {}
                LP.MenuItem {
                    text: Applet.colorThemeNames[11]
                    checkable: true
                    checked: Applet.colorTheme === 11
                    onTriggered: Applet.colorTheme = 11
                }
            }
            LP.Menu {
                id: customColorMenu
                title: qsTr("自定义配色")
                LP.Menu {
                    id: fontColorMenu
                    title: qsTr("字体颜色")
                    LP.MenuItem {
                        text: qsTr("跟随主题")
                        checkable: true
                        checked: Applet.customTextColor === ""
                        onTriggered: { Applet.customTextColor = ""; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("纯白 #FFFFFF")
                        checkable: true
                        checked: Applet.customTextColor === "#FFFFFF"
                        onTriggered: { Applet.customTextColor = "#FFFFFF"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("柠檬黄 #FFD75E")
                        checkable: true
                        checked: Applet.customTextColor === "#FFD75E"
                        onTriggered: { Applet.customTextColor = "#FFD75E"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("苹果青 #7EE0A3")
                        checkable: true
                        checked: Applet.customTextColor === "#7EE0A3"
                        onTriggered: { Applet.customTextColor = "#7EE0A3"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("冰川蓝 #8FD3FF")
                        checkable: true
                        checked: Applet.customTextColor === "#8FD3FF"
                        onTriggered: { Applet.customTextColor = "#8FD3FF"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("霓虹紫 #C792EA")
                        checkable: true
                        checked: Applet.customTextColor === "#C792EA"
                        onTriggered: { Applet.customTextColor = "#C792EA"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("樱花粉 #FF9EC7")
                        checkable: true
                        checked: Applet.customTextColor === "#FF9EC7"
                        onTriggered: { Applet.customTextColor = "#FF9EC7"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("珊瑚红 #FF7A6E")
                        checkable: true
                        checked: Applet.customTextColor === "#FF7A6E"
                        onTriggered: { Applet.customTextColor = "#FF7A6E"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("暖阳橙 #FFB877")
                        checkable: true
                        checked: Applet.customTextColor === "#FFB877"
                        onTriggered: { Applet.customTextColor = "#FFB877"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("浅灰 #B9BEC9")
                        checkable: true
                        checked: Applet.customTextColor === "#B9BEC9"
                        onTriggered: { Applet.customTextColor = "#B9BEC9"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("深灰 #3A3F47")
                        checkable: true
                        checked: Applet.customTextColor === "#3A3F47"
                        onTriggered: { Applet.customTextColor = "#3A3F47"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("墨黑 #101216")
                        checkable: true
                        checked: Applet.customTextColor === "#101216"
                        onTriggered: { Applet.customTextColor = "#101216"; Applet.colorTheme = 11 }
                    }
                }
                LP.Menu {
                    id: bgColorMenu
                    title: qsTr("背景颜色")
                    LP.MenuItem {
                        text: qsTr("无背景（跟随系统）")
                        checkable: true
                        checked: Applet.customBgColor === ""
                        onTriggered: { Applet.customBgColor = ""; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("半透明黑 #59000000")
                        checkable: true
                        checked: Applet.customBgColor === "#59000000"
                        onTriggered: { Applet.customBgColor = "#59000000"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("曜石黑 #CC171B22")
                        checkable: true
                        checked: Applet.customBgColor === "#CC171B22"
                        onTriggered: { Applet.customBgColor = "#CC171B22"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("深空蓝 #CC142340")
                        checkable: true
                        checked: Applet.customBgColor === "#CC142340"
                        onTriggered: { Applet.customBgColor = "#CC142340"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("暮光紫 #CC251C3E")
                        checkable: true
                        checked: Applet.customBgColor === "#CC251C3E"
                        onTriggered: { Applet.customBgColor = "#CC251C3E"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("墨夜绿 #CC0F2A1D")
                        checkable: true
                        checked: Applet.customBgColor === "#CC0F2A1D"
                        onTriggered: { Applet.customBgColor = "#CC0F2A1D"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("纯黑 #E6000000")
                        checkable: true
                        checked: Applet.customBgColor === "#E6000000"
                        onTriggered: { Applet.customBgColor = "#E6000000"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("浅灰 #B3E0E0E0")
                        checkable: true
                        checked: Applet.customBgColor === "#B3E0E0E0"
                        onTriggered: { Applet.customBgColor = "#B3E0E0E0"; Applet.colorTheme = 11 }
                    }
                    LP.MenuItem {
                        text: qsTr("纯白 #E6FFFFFF")
                        checkable: true
                        checked: Applet.customBgColor === "#E6FFFFFF"
                        onTriggered: { Applet.customBgColor = "#E6FFFFFF"; Applet.colorTheme = 11 }
                    }
                }
            }

            LP.MenuSeparator {}
            LP.MenuItem {
                text: root.sourceLabel
                enabled: false
            }
            LP.MenuItem {
                text: qsTr("刷新歌词")
                onTriggered: Applet.refreshLyrics()
            }
        }
    }
}