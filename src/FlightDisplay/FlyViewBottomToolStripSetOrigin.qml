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

    property string confirmMessage:     qsTr("确认设置原点坐标？")
    property real   _margins:          ScreenTools.defaultFontPixelWidth / 2
    
    // 当用户点击确认按钮时触发的信号
    signal originConfirmed(double latitude, double longitude, double altitude)
    
    function show() {
        visible = true
        latField.forceActiveFocus()
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
        
        // 坐标输入区域
        GridLayout {
            Layout.fillWidth:   true
            columns:            2
            rowSpacing:         _margins / 2
            columnSpacing:      _margins
            
            QGCLabel {
                text:           qsTr("纬度:")
                font.pointSize: ScreenTools.defaultFontPointSize
            }
            
            TextField {
                id:                 latField
                Layout.fillWidth:   true
                placeholderText:    qsTr("输入纬度")
                selectByMouse:      true
                validator:          DoubleValidator {}
                inputMethodHints:   Qt.ImhDigitsOnly
            }
            
            QGCLabel {
                text:           qsTr("经度:")
                font.pointSize: ScreenTools.defaultFontPointSize
            }
            
            TextField {
                id:                 lonField
                Layout.fillWidth:   true
                placeholderText:    qsTr("输入经度")
                selectByMouse:      true
                validator:          DoubleValidator {}
                inputMethodHints:   Qt.ImhDigitsOnly
            }
            
            QGCLabel {
                text:           qsTr("高度:")
                font.pointSize: ScreenTools.defaultFontPointSize
            }
            
            TextField {
                id:                 altField
                Layout.fillWidth:   true
                placeholderText:    qsTr("输入高度")
                selectByMouse:      true
                validator:          DoubleValidator {}
                inputMethodHints:   Qt.ImhDigitsOnly
            }
        }

        RowLayout {
            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelWidth
            
            // 确认按钮
            QGCButton {
                text:               qsTr("确认")
                Layout.fillWidth:   true
                enabled:            latField.text.length > 0 && lonField.text.length > 0 && altField.text.length > 0
                onClicked: {
                    // 获取输入的坐标值
                    var lat = parseFloat(latField.text) || 0
                    var lon = parseFloat(lonField.text) || 0
                    var alt = parseFloat(altField.text) || 0
                    
                    hide()
                    // 发出确认信号，传递坐标值
                    _root.originConfirmed(lat, lon, alt)
                }
            }
            
            // 取消按钮
            QGCButton {
                text:               qsTr("取消")
                Layout.fillWidth:   true
                onClicked:          hide()
            }
        }
    }
} 