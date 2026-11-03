import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1240
    height: 850
    minimumWidth: 980
    minimumHeight: 690
    title: "KeyFlux Studio"
    color: "#090e1b"
    Material.theme: Material.Dark
    Material.accent: Material.Cyan

    readonly property color textPrimary: "#edf3ff"
    readonly property color textMuted: "#8795b0"
    readonly property color surface: "#111a2e"
    readonly property color outline: "#233250"
    readonly property color blue: "#648efb"
    property real phase: 0.0

    readonly property var multiModes: [
        "关闭", "呼吸灯", "静态常亮", "空间彩虹 · 左 → 右", "空间彩虹 · 上 → 下"
    ]
    readonly property var keychronModes: [
        "关闭", "纯色", "呼吸", "螺旋波纹", "全局循环", "左右循环",
        "上下循环", "移动箭头", "外向波纹", "双向波纹", "旋转风车",
        "螺旋", "双信标", "彩虹信标", "果冻雨滴", "像素雨",
        "打字热力图", "数字雨", "按键反应", "多点反应", "多点连锁",
        "水花", "纯色水花", "单键 RGB", "混合 RGB"
    ]

    readonly property var keyNames: [
        ["~","1","2","3","4","5","6","7","8","9","0","-","=","Back","Ins","Home","PgUp"],
        ["Tab","Q","W","E","R","T","Y","U","I","O","P","[","]","\\","Del","End","PgDn"],
        ["Caps","A","S","D","F","G","H","J","K","L",";","'","Enter"],
        ["Shift","Z","X","C","V","B","N","M",",",".","/","Shift","↑"],
        ["Ctrl","Win","Alt","Space","Alt","Fn","Win","Ctrl","←","↓","→"]
    ]
    readonly property var keyIds: [
        [55,56,57,58,39,40,41,42,24,25,26,10,11,12,0,1,2],
        [59,60,61,43,44,45,46,27,28,29,30,13,14,15,3,4,5],
        [62,63,64,47,48,49,50,31,32,33,16,17,18],
        [65,66,67,51,52,53,35,36,37,38,20,21,9],
        [68,69,70,54,34,22,23,19,6,7,8]
    ]

    function previewColor(column, row, totalColumns) {
        if (backend.mode === 0) return "#202a40"
        if (backend.mode === 2 || (backend.selectedDevice === 1 && backend.mode === 1))
            return backend.color
        if (backend.selectedDevice === 0 && backend.mode === 1) return "#784c78"
        var h = 0
        if ((backend.selectedDevice === 0 && backend.mode === 4)
            || (backend.selectedDevice === 1 && backend.mode === 6))
            h = (row / 5.0 - phase) % 1.0
        else h = (column / totalColumns - phase) % 1.0
        if (h < 0) h += 1
        return Qt.hsla(h, 0.77, 0.59, 1.0)
    }

    Timer {
        interval: 100
        running: root.visible
        repeat: true
        onTriggered: root.phase = (root.phase + 0.005 * Math.max(1, backend.speed / 6)) % 1.0
    }

    component ActionButton: Button {
        id: action
        property bool primary: false
        font.family: "Segoe UI"
        font.pixelSize: 13
        font.weight: Font.DemiBold
        height: 40
        padding: 12
        background: Rectangle {
            radius: 10
            color: !action.enabled ? "#273047"
                : action.down ? (action.primary ? "#4b73db" : "#273751")
                : action.hovered ? (action.primary ? "#779cfb" : "#243651")
                : action.primary ? "#5b82ee" : "#1a2942"
            border.color: action.primary ? "#779aff" : "#2a3e5d"
            border.width: 1
            Behavior on color { ColorAnimation { duration: 140 } }
        }
        contentItem: Text {
            text: action.text
            font: action.font
            color: action.enabled ? "#f0f5ff" : "#76839c"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component Panel: Rectangle {
        color: root.surface
        border.color: root.outline
        border.width: 1
        radius: 17
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 244
            color: "#0d1425"
            border.color: "#1a2842"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 19
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    spacing: 11
                    Rectangle {
                        width: 43; height: 43; radius: 13
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#75a5ff" }
                            GradientStop { position: 1; color: "#745ef5" }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "K"
                            color: "white"
                            font.pixelSize: 25
                            font.weight: Font.Black
                        }
                    }
                    Column {
                        spacing: 1
                        Text { text: "KeyFlux"; color: root.textPrimary; font.pixelSize: 22; font.bold: true; font.family: "Segoe UI" }
                        Text { text: "STUDIO  /  0.1"; color: root.textMuted; font.pixelSize: 10; font.letterSpacing: 1.5 }
                    }
                }

                Text {
                    Layout.topMargin: 22
                    Layout.bottomMargin: 12
                    text: "我的设备"
                    color: root.textMuted
                    font.pixelSize: 12
                    font.bold: true
                }
                Repeater {
                    model: 2
                    delegate: Rectangle {
                        id: deviceCard
                        readonly property int deviceIndex: index
                        readonly property bool deviceConnected: index === 0 ? backend.multiConnected : backend.keychronConnected
                        Layout.fillWidth: true
                        Layout.preferredHeight: 91
                        Layout.bottomMargin: 10
                        radius: 12
                        color: backend.selectedDevice === index ? "#1a2b4d" : "#131e33"
                        border.color: backend.selectedDevice === index ? "#5f85ee" : "#273751"
                        border.width: 1
                        Row {
                            anchors.centerIn: parent
                            width: parent.width - 23
                            spacing: 11
                            Rectangle {
                                width: 37; height: 42; radius: 8
                                color: "#263755"
                                border.color: "#4a628b"
                                Text { anchors.centerIn: parent; text: "⌨"; font.pixelSize: 23; color: "#b6cfff" }
                            }
                            Column {
                                spacing: 5
                                Text {
                                    text: deviceCard.deviceIndex === 0 ? "MultiPad" : "Keychron V6 Max"
                                    color: root.textPrimary
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                Row {
                                    spacing: 6
                                    Rectangle {
                                        width: 7; height: 7; radius: 4
                                        color: deviceCard.deviceConnected ? "#4ce3ae" : "#69768d"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: deviceCard.deviceConnected ? "已连接" :
                                            deviceCard.deviceIndex === 1 && backend.keychronDetected ? "接口未就绪" : "未连接"
                                        color: deviceCard.deviceConnected ? "#6ce7b8" : root.textMuted
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.selectedDevice = deviceCard.deviceIndex
                        }
                    }
                }

                Rectangle { Layout.topMargin: 12; Layout.fillWidth: true; height: 1; color: root.outline }
                RowLayout {
                    Layout.topMargin: 19
                    Layout.fillWidth: true
                    Text { text: "●"; color: "#8f9fbe"; font.pixelSize: 15 }
                    Text { text: "灯光与设备控制"; color: root.textPrimary; font.pixelSize: 13 }
                }
                RowLayout {
                    Layout.topMargin: 18
                    Layout.fillWidth: true
                    Text { text: "◇"; color: "#8f9fbe"; font.pixelSize: 15 }
                    Text { text: "实时诊断与日志"; color: root.textMuted; font.pixelSize: 13 }
                }

                Item { Layout.fillHeight: true }

                Panel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 115
                    color: "#14223a"
                    Column {
                        anchors.fill: parent; anchors.margins: 13; spacing: 6
                        Text { text: "USB HID · 本地连接"; color: "#b6ceff"; font.bold: true; font.pixelSize: 12 }
                        Text {
                            text: "所有指令直接通过 USB 发送，\n无需网络或浏览器。"
                            color: root.textMuted
                            font.pixelSize: 11
                            lineHeight: 1.4
                        }
                    }
                }
                Text {
                    Layout.topMargin: 17
                    Layout.alignment: Qt.AlignHCenter
                    text: "© KeyFlux Studio"
                    color: "#536380"
                    font.pixelSize: 10
                }
            }
        }

        ScrollView {
            id: mainScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            contentWidth: availableWidth

            ColumnLayout {
                width: mainScroll.availableWidth
                spacing: 18

                Item { Layout.preferredHeight: 8 }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 30
                    Layout.rightMargin: 30
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: backend.selectedDevice === 0 ? "MultiPad · 灯光工作台" : "Keychron V6 Max · 灯光工作台"
                            color: root.textPrimary
                            font.pixelSize: 26
                            font.bold: true
                            font.family: "Segoe UI"
                        }
                        Text { text: backend.info; color: root.textMuted; font.pixelSize: 12 }
                    }
                    ActionButton { text: "↻ 刷新状态"; enabled: backend.connected; onClicked: backend.refresh() }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.leftMargin: 30
                    Layout.rightMargin: 30
                    Layout.preferredHeight: backend.selectedDevice === 0 ? 313 : 275
                    color: "#131f35"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 13

                        Row {
                            width: parent.width
                            spacing: 12
                            Text { text: "键盘灯效预览"; color: root.textPrimary; font.pixelSize: 16; font.bold: true }
                            Rectangle {
                                width: 100; height: 22; radius: 11; color: "#23395a"
                                Text {
                                    anchors.centerIn: parent
                                    text: backend.selectedDevice === 0 ? "71 颗已映射" : "RGB Matrix"
                                    color: "#aaccff"; font.pixelSize: 10
                                }
                            }
                        }
                        Text {
                            text: backend.selectedDevice === 0 ?
                                "按实际 PCB 位置排列 · 点击按键可单颗定位（红色）" :
                                "V6 Max 灯光示意 · 具体颜色以键盘实物为准"
                            font.pixelSize: 11
                            color: root.textMuted
                        }

                        Flickable {
                            id: previewFlick
                            width: parent.width
                            height: backend.selectedDevice === 0 ? 223 : 176
                            contentWidth: 1020
                            contentHeight: height
                            clip: true
                            interactive: true
                            boundsBehavior: Flickable.StopAtBounds
                            Rectangle {
                                width: 1020; height: previewFlick.height; radius: 12; color: "#0b1221"
                                border.color: "#1f3150"
                                Column {
                                    anchors.fill: parent; anchors.margins: 12; spacing: 6
                                    visible: backend.selectedDevice === 0
                                    Repeater {
                                        model: root.keyNames.length
                                        delegate: Row {
                                            id: keyRow
                                            property int rowIndex: index
                                            spacing: 6
                                            Repeater {
                                                model: root.keyNames[keyRow.rowIndex].length
                                                delegate: Rectangle {
                                                    id: keyCap
                                                    readonly property int ledIndex: root.keyIds[keyRow.rowIndex][index]
                                                    readonly property string label: root.keyNames[keyRow.rowIndex][index]
                                                    width: label === "Space" ? 136 : label === "Shift" ? 72 : 48
                                                    height: 35
                                                    radius: 6
                                                    color: root.previewColor(index, keyRow.rowIndex, root.keyNames[keyRow.rowIndex].length)
                                                    border.color: "#597194"
                                                    border.width: 1
                                                    opacity: backend.mode === 0 ? 0.55 : 0.90
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: keyCap.label
                                                        font.pixelSize: keyCap.label.length > 4 ? 9 : 11
                                                        font.bold: true
                                                        color: "#0b1425"
                                                    }
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        enabled: backend.multiConnected
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: backend.testLed(keyCap.ledIndex)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                Grid {
                                    anchors.centerIn: parent
                                    columns: 18
                                    rowSpacing: 5
                                    columnSpacing: 6
                                    visible: backend.selectedDevice === 1
                                    Repeater {
                                        model: 108
                                        delegate: Rectangle {
                                            width: 44; height: 22; radius: 4
                                            color: root.previewColor(index % 18, Math.floor(index / 18), 18)
                                            border.color: "#526a90"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 30
                    Layout.rightMargin: 30
                    spacing: 18

                    Panel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 317
                        Layout.minimumWidth: 375

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 19
                            spacing: 13
                            Text { text: "灯效参数"; color: root.textPrimary; font.pixelSize: 17; font.bold: true }
                            Text { text: "灯光模式"; color: root.textMuted; font.pixelSize: 12 }
                            ComboBox {
                                id: modeBox
                                Layout.fillWidth: true
                                enabled: backend.connected
                                model: backend.selectedDevice === 0 ? root.multiModes : root.keychronModes
                                currentIndex: Math.max(0, Math.min(count - 1, backend.mode))
                                onActivated: backend.setMode(currentIndex)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "亮度"; color: root.textMuted; font.pixelSize: 12 }
                                Item { Layout.fillWidth: true }
                                Text { text: brightnessSlider.value.toFixed(0) + " / 255"; color: "#d1e0ff"; font.pixelSize: 12 }
                            }
                            Slider {
                                id: brightnessSlider
                                Layout.fillWidth: true
                                from: 0; to: 255; stepSize: 1
                                value: backend.brightness
                                enabled: backend.connected
                                onMoved: backend.setBrightness(Math.round(value))
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "动画速度"; color: root.textMuted; font.pixelSize: 12 }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: speedSlider.value.toFixed(0) +
                                        (backend.selectedDevice === 0 ? " / 10" : " / 255")
                                    color: "#d1e0ff"; font.pixelSize: 12
                                }
                            }
                            Slider {
                                id: speedSlider
                                Layout.fillWidth: true
                                from: backend.selectedDevice === 0 ? 1 : 0
                                to: backend.selectedDevice === 0 ? 10 : 255
                                stepSize: 1
                                value: backend.speed
                                enabled: backend.connected
                                onMoved: backend.setSpeed(Math.round(value))
                            }
                        }
                    }

                    Panel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 317
                        Layout.minimumWidth: 290

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 19
                            spacing: 14
                            Text { text: "颜色与存储"; color: root.textPrimary; font.pixelSize: 17; font.bold: true }
                            Text { text: "基础颜色（静态 / 呼吸等模式）"; color: root.textMuted; font.pixelSize: 12 }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Rectangle {
                                    width: 46; height: 42; radius: 10
                                    color: backend.color
                                    border.color: "#6077a0"
                                }
                                TextField {
                                    id: colorField
                                    Layout.fillWidth: true
                                    text: backend.color
                                    maximumLength: 7
                                    placeholderText: "#RRGGBB"
                                    enabled: backend.connected
                                    onAccepted: backend.setColor(text)
                                }
                                ActionButton {
                                    text: "应用"
                                    enabled: backend.connected
                                    onClicked: backend.setColor(colorField.text)
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: root.outline }
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "开机自动恢复"; color: root.textPrimary; font.pixelSize: 13 }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: backend.selectedDevice === 0 ?
                                        (backend.saved ? "已保存" : "待保存") : "VIA EEPROM"
                                    color: backend.saved ? "#6ee4ae" : "#a7b8d5"
                                    font.pixelSize: 12
                                }
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: "▣  保存当前灯效到键盘"
                                primary: true
                                enabled: backend.connected
                                onClicked: backend.save()
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: root.textMuted
                                font.pixelSize: 11
                                lineHeight: 1.3
                                text: backend.selectedDevice === 0 ?
                                    "Flash 双页保存。下次插拔 USB 或重启 STM32 后恢复。" :
                                    "通过 VIA 命令保存到 Keychron。需要有线 USB 与兼容固件。"
                            }
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.leftMargin: 30
                    Layout.rightMargin: 30
                    Layout.preferredHeight: 125
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 18; spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "诊断与调试"; color: root.textPrimary; font.pixelSize: 16; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: backend.selectedDevice === 0 ?
                                    "可测试物理 RGB 数据链 0–99" :
                                    "QMK 自定义日志需要定制固件"
                                color: root.textMuted; font.pixelSize: 11
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Switch {
                                id: logToggle
                                text: "USB 日志"
                                checked: backend.loggingEnabled
                                enabled: backend.connected &&
                                    (backend.selectedDevice === 0 || backend.logControlSupported)
                                onClicked: backend.setLogging(checked)
                            }
                            Item { Layout.fillWidth: true }
                            ActionButton {
                                text: "停止单灯测试"
                                enabled: backend.selectedDevice === 0 && backend.multiConnected
                                onClicked: backend.stopTest()
                            }
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.leftMargin: 30
                    Layout.rightMargin: 30
                    Layout.preferredHeight: 206
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 17; spacing: 9
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "运行日志"; color: root.textPrimary; font.pixelSize: 16; font.bold: true }
                            Item { Layout.fillWidth: true }
                            ActionButton { text: "清空"; onClicked: backend.clearLogs() }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 9
                            color: "#080e1c"
                            border.color: "#253857"
                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 7
                                clip: true
                                TextArea {
                                    readOnly: true
                                    text: backend.logs
                                    wrapMode: TextArea.WrapAnywhere
                                    color: "#9ccbb9"
                                    font.family: "Consolas"
                                    font.pixelSize: 11
                                    selectByMouse: true
                                    background: Item {}
                                }
                            }
                        }
                    }
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 24
                    text: "KeyFlux Studio · C++17 / Qt Quick / HIDAPI · 本地运行"
                    color: "#536380"; font.pixelSize: 11
                }
            }
        }
    }
}
