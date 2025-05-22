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

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.Palette

// 通用的底部工具栏按钮，可用于创建不同的按钮
Item {
    id:                     toolStripButton
    anchors.top:            parent.top
    anchors.bottom:         parent.bottom
    width:                  height
    
    // 按钮的通用属性
    property string buttonId:       ""          // 按钮的唯一标识符，用于执行操作
    property string iconSource:     ""          // 图标路径
    property string buttonText:     ""          // 显示文本
    property var    fontSize:       ScreenTools.smallFontPointSize  // 文本大小
    
    // 按钮被点击时的信号，传递按钮ID
    signal buttonClicked(string buttonId)
    
    property bool hovered:  buttonMouseArea.containsMouse
    
    // 按钮背景
    Rectangle {
        id: buttonBkRect
        anchors.fill:   parent
        radius:         ScreenTools.defaultFontPixelWidth / 2
        color:          toolStripButton.hovered ? qgcPal.toolStripHoverColor : qgcPal.toolbarBackground
    }
    
    // 内容布局
    Column {
        anchors.centerIn: parent
        spacing: ScreenTools.defaultFontPixelWidth * 0.1
        
        QGCColoredImage {
            height:                     parent.parent.height * 0.5
            width:                      parent.parent.width * 0.5
            source:                     iconSource
            color:                      qgcPal.buttonText
            fillMode:                   Image.PreserveAspectFit
            sourceSize.height:          height
            sourceSize.width:           width
            anchors.horizontalCenter:   parent.horizontalCenter
        }
        
        QGCLabel {
            text:                       buttonText
            color:                      qgcPal.buttonText
            font.pointSize:             fontSize
            anchors.horizontalCenter:   parent.horizontalCenter
        }
    }
    
    // 鼠标事件处理
    MouseArea {
        id:             buttonMouseArea
        anchors.fill:   parent
        hoverEnabled:   true
        
        onClicked: {
            toolStripButton.buttonClicked(buttonId)
        }
    }
    
    // 颜色设置
    QGCPalette { id: qgcPal; colorGroupEnabled: true }
} 