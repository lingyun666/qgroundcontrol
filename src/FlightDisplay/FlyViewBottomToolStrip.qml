/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightDisplay
import QGroundControl.ScreenTools
import QGroundControl.Palette
import QGroundControl.Controllers

Rectangle {
    id:         _root
    color:      qgcPal.toolbarBackground
    height:     ScreenTools.defaultFontPixelHeight * 3
    width:      Math.min(maxWidth, toolStripRow.width + (toolStripRow.anchors.margins * 2))
    radius:     ScreenTools.defaultFontPixelWidth / 2

    property real   maxWidth:           ScreenTools.defaultFontPixelWidth * 30
    property var    fontSize:           ScreenTools.smallFontPointSize
    
    // 添加CustomCommandSender
    property var    _commandSender:     CustomCommandSender {}
    
    // 当前需要确认的按钮ID
    property string _currentButtonID:   ""
    
    // 确认框中显示的信息
    property var    confirmMessages: {
        "activeButton": qsTr("激活所选功能"),
        "originButton": qsTr("设置原点坐标"),
        // 在这里添加更多按钮的确认信息
        "otherButton": qsTr("执行其他功能")
    }
    
    // 连接确认对话框的confirmed信号
    Connections {
        target: bottomToolStripConfirm
        
        function onConfirmed() {
            // 根据当前按钮ID执行相应的操作
            executeButtonAction(_currentButtonID)
        }
    }
    
    // 连接原点设置对话框的originConfirmed信号
    Connections {
        target: bottomToolStripSetOrigin
        
        function onOriginConfirmed(latitude, longitude, altitude) {
            console.log("设置原点坐标: lat=" + latitude + ", lon=" + longitude + ", alt=" + altitude)
            _commandSender.sendCommandByUdp(
                0,      // compId (目标组件ID)
                31011,  // MAV_CMD_USER_2
                true,   // showError
                1.0,    // param1 = 1.0 表示使用自定义坐标
                latitude, longitude, altitude, 0, 0, 0  // 坐标参数
            )
        }
    }
    
    // 根据按钮ID执行相应的操作
    function executeButtonAction(buttonID) {
        console.log("执行按钮操作: " + buttonID)
        
        switch(buttonID) {
        case "activeButton":
            // 执行Active按钮的操作
            console.log("Execute Active function")
            _commandSender.sendCommandByUdp(
                0,      // compId (目标组件ID)
                31010,  // MAV_CMD_USER_1
                true,   // showError
                1.0,    // param1 = 1.0
                0, 0, 0, 0, 0, 0  // 其他参数
            )
            break
        default:
            console.log("未知的按钮ID: " + buttonID)
        }
    }
    
    // 显示确认对话框
    function showConfirmDialog(buttonID) {
        _currentButtonID = buttonID
        var message = confirmMessages[buttonID] || qsTr("确认执行此操作？")
        bottomToolStripConfirm.show(message)
    }


    // 显示设置原点对话框
    function showOriginInputDialog() {
        // 对于Origin按钮，显示坐标设置对话框
        console.log("Show Origin coordinates dialog")
        bottomToolStripSetOrigin.show()
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    QGCFlickable {
        id:                 toolStripFlickable
        anchors.margins:    ScreenTools.defaultFontPixelWidth * 0.2
        anchors.top:        parent.top
        anchors.left:       parent.left
        anchors.right:      parent.right
        height:             parent.height - anchors.margins * 2
        contentWidth:       toolStripRow.width
        flickableDirection: Flickable.HorizontalFlick
        clip:               true

        Row {
            id:             toolStripRow
            anchors.top:    parent.top
            anchors.bottom: parent.bottom
            spacing:        ScreenTools.defaultFontPixelWidth * 0.125

            // 使用独立的Active按钮组件
            FlyViewBottomToolStripActiveButton {
                fontSize: _root.fontSize
                
                onButtonClicked: function(buttonId) {
                    showConfirmDialog(buttonId)
                }
            }
            
            // 添加Origin按钮
            FlyViewBottomToolStripOriginButton {
                fontSize: _root.fontSize
                
                onButtonClicked: function(buttonId) {
                    showOriginInputDialog()
                }
            }
            
            // 在这里可以添加更多按钮
            // 例如:
            /*
            FlyViewBottomToolStripButton {
                buttonId:       "otherButton"
                iconSource:     "/res/otherbuttonicon.svg"
                buttonText:     qsTr("Other")
                fontSize:       _root.fontSize
                
                onButtonClicked: function(buttonId) {
                    showConfirmDialog(buttonId)
                }
            }
            */
        }
    }

    property real bottomEdgeCenterInset: visible ? height + (ScreenTools.defaultFontPixelWidth * 0.2) : 0
} 