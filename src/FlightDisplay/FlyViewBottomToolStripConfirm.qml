/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.Controllers

Rectangle {
    id:         _root
    width:      ScreenTools.defaultFontPixelWidth * 35
    height:     mainLayout.height + (_margins * 2)
    radius:     ScreenTools.defaultFontPixelWidth / 2
    color:      qgcPal.window
    visible:    false
    z:          QGroundControl.zOrderTopMost

    property string confirmMessage:     qsTr("确认执行此操作？")
    property real   _margins:          ScreenTools.defaultFontPixelWidth / 2
    
    // 当用户点击确认滑条时触发的信号
    signal confirmed
    
    function show(message) {
        if (message) {
            confirmMessage = message
        }
        visible = true
        slider.focus = true
    }
    
    function hide() {
        visible = false
    }

    QGCPalette { id: qgcPal }

    ColumnLayout {
        id:                 mainLayout
        anchors.centerIn:   parent
        width:              parent.width - (_margins * 2)
        spacing:            _margins

        QGCLabel {
            Layout.fillWidth:       true
            horizontalAlignment:    Text.AlignHCenter
            wrapMode:               Text.WordWrap
            font.pointSize:         ScreenTools.defaultFontPointSize
            font.bold:              true
            text:                   confirmMessage
        }

        RowLayout {
            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelWidth

            SliderSwitch {
                id:                 slider
                confirmText:        ScreenTools.isMobile ? qsTr("滑动以确认") : qsTr("滑动或按住空格键")
                Layout.fillWidth:   true

                onAccept: {
                    hide()
                    // 发出确认信号，让底部工具栏处理后续逻辑
                    _root.confirmed()
                }
            }

            Rectangle {
                height: slider.height * 0.75
                width:  height
                radius: height / 2
                color:  qgcPal.primaryButton

                QGCColoredImage {
                    anchors.margins:    parent.height / 4
                    anchors.fill:       parent
                    source:             "/res/XDelete.svg"
                    fillMode:           Image.PreserveAspectFit
                    color:              qgcPal.text
                }

                QGCMouseArea {
                    fillItem:   parent
                    onClicked:  hide()
                }
            }
        }
    }
} 