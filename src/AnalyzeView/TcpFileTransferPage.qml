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
import Qt.labs.qmlmodels

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Controllers
import QGroundControl.ScreenTools

AnalyzePage {
    id: tcpFileTransferPage
    pageComponent: pageComponent
    pageDescription: qsTr("NX文件传输使您能够从远程服务器下载文件。点击连接并获取服务器上可用的文件列表。")
    allowPopout: true

    Component {
        id: pageComponent

        Item {
            width: availableWidth
            height: availableHeight

            TcpFileTransferController {
                id: controller
            }

            // 主滚动区域
            Flickable {
                id: mainFlickable
                anchors.fill: parent
                contentWidth: width
                contentHeight: mainLayout.height
                flickableDirection: Flickable.VerticalFlick
                clip: true

                ColumnLayout {
                    id: mainLayout
                    width: parent.width
                    spacing: ScreenTools.defaultFontPixelHeight

                    // 1. 连接设置区域
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: connectionLayout.height + ScreenTools.defaultFontPixelHeight
                        color: qgcPal.windowShade
                        radius: 3

                        ColumnLayout {
                            id: connectionLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: ScreenTools.defaultFontPixelWidth
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: ScreenTools.defaultFontPixelHeight / 2

                            QGCLabel {
                                Layout.fillWidth: true
                                text: qsTr("连接设置")
                                font.bold: true
                                font.pixelSize: ScreenTools.defaultFontPixelSize * 1.1
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 4
                                rowSpacing: ScreenTools.defaultFontPixelHeight / 2
                                columnSpacing: ScreenTools.defaultFontPixelWidth

                                QGCLabel { 
                                    text: qsTr("服务器地址:") 
                                    Layout.alignment: Qt.AlignRight
                                }
                                
                                QGCTextField {
                                    id: serverAddressField
                                    Layout.fillWidth: true
                                    text: controller.serverAddress
                                    enabled: !controller.connected
                                    onEditingFinished: controller.serverAddress = text
                                }

                                QGCLabel { 
                                    text: qsTr("端口:") 
                                    Layout.alignment: Qt.AlignRight
                                }
                                
                                QGCTextField {
                                    id: serverPortField
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                    text: controller.serverPort
                                    enabled: !controller.connected
                                    validator: IntValidator { bottom: 1; top: 65535 }
                                    onEditingFinished: controller.serverPort = text
                                }

                                QGCLabel { 
                                    text: qsTr("保存目录:") 
                                    Layout.alignment: Qt.AlignRight
                                }
                                
                                QGCComboBox {
                                    id: folderComboBox
                                    Layout.columnSpan: 3
                                    Layout.fillWidth: true
                                    model: controller.downloadFolders
                                    currentIndex: controller.downloadFolders.indexOf(controller.selectedFolder)
                                    onActivated: controller.selectedFolder = currentText
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: ScreenTools.defaultFontPixelWidth
                                
                                Item { Layout.fillWidth: true } // 弹簧
                                
                                QGCButton {
                                    text: controller.connected ? qsTr("断开") : qsTr("连接")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                    onClicked: controller.connected ? controller.disconnect() : controller.connect()
                                }

                                QGCButton {
                                    text: qsTr("刷新")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                    enabled: controller.connected && !controller.downloading
                                    onClicked: controller.refresh()
                                }
                            }
                        }
                    }

                    // 2. 可用文件列表区域
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(fileListLayout.implicitHeight, parent.width * 0.4)
                        color: qgcPal.windowShade
                        radius: 3

                        ColumnLayout {
                            id: fileListLayout
                            anchors.fill: parent
                            anchors.margins: ScreenTools.defaultFontPixelWidth
                            spacing: ScreenTools.defaultFontPixelHeight / 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    text: qsTr("可用文件列表")
                                    font.bold: true
                                    font.pixelSize: ScreenTools.defaultFontPixelSize * 1.1
                                }

                                Item { Layout.fillWidth: true } // 弹簧

                                QGCButton {
                                    text: qsTr("全选")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 8
                                    enabled: controller.files.length > 0 && !controller.downloading
                                    onClicked: {
                                        controller.selectAll()
                                        selectAllCheckBox.checked = true
                                    }
                                }

                                QGCButton {
                                    text: qsTr("取消全选")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 8
                                    enabled: controller.files.length > 0 && !controller.downloading
                                    onClicked: {
                                        controller.clearSelection()
                                        selectAllCheckBox.checked = false
                                    }
                                }
                                
                                QGCButton {
                                    text: qsTr("下载")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 8
                                    enabled: controller.connected && !controller.downloading
                                    onClicked: controller.download()
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: qgcPal.window
                                border.color: qgcPal.text
                                border.width: 1
                                clip: true

                                QGCFlickable {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    contentWidth: fileGrid.width
                                    contentHeight: fileGrid.height
                                    flickableDirection: Flickable.VerticalFlick
                                    clip: true

                                    GridLayout {
                                        id: fileGrid
                                        width: parent.width
                                        columns: 5
                                        columnSpacing: ScreenTools.defaultFontPixelWidth
                                        rowSpacing: ScreenTools.defaultFontPixelHeight / 2

                                        // 标题行
                                        QGCCheckBox {
                                            id: selectAllCheckBox
                                            enabled: controller.files.length > 0 && !controller.downloading
                                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 3
                                        }

                                        QGCLabel { 
                                            text: qsTr("文件名")
                                            Layout.fillWidth: true
                                            font.bold: true
                                        }
                                        
                                        QGCLabel { 
                                            text: qsTr("大小") 
                                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                            font.bold: true
                                        }
                                        
                                        QGCLabel { 
                                            text: qsTr("状态") 
                                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                            font.bold: true
                                        }
                                        
                                        QGCLabel { 
                                            text: qsTr("进度") 
                                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 15
                                            font.bold: true
                                        }

                                        // 文件列表
                                        Repeater {
                                            model: controller.files

                                            QGCCheckBox {
                                                enabled: !controller.downloading
                                                checked: modelData.selected
                                                onClicked: modelData.selected = checked
                                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 3
                                            }
                                        }

                                        Repeater {
                                            model: controller.files
                                            QGCLabel { 
                                                text: modelData.name 
                                                Layout.fillWidth: true
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        Repeater {
                                            model: controller.files
                                            QGCLabel { 
                                                text: modelData.sizeStr 
                                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                            }
                                        }

                                        Repeater {
                                            model: controller.files
                                            QGCLabel { 
                                                text: modelData.status 
                                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                            }
                                        }

                                        Repeater {
                                            model: controller.files
                                            ProgressBar {
                                                value: modelData.progress / 100
                                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 15
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 3. 下载进度和下载好的文件区域
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: downloadStatusLayout.implicitHeight + ScreenTools.defaultFontPixelHeight
                        color: qgcPal.windowShade
                        radius: 3

                        ColumnLayout {
                            id: downloadStatusLayout
                            anchors.fill: parent
                            anchors.margins: ScreenTools.defaultFontPixelWidth
                            spacing: ScreenTools.defaultFontPixelHeight / 2

                            QGCLabel {
                                Layout.fillWidth: true
                                text: qsTr("下载状态")
                                font.bold: true
                                font.pixelSize: ScreenTools.defaultFontPixelSize * 1.1
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 3
                                color: qgcPal.window
                                border.color: qgcPal.text
                                border.width: 1

                                QGCLabel {
                                    anchors.fill: parent
                                    anchors.margins: ScreenTools.defaultFontPixelWidth
                                    text: controller.downloading ? 
                                          qsTr("正在下载文件...") : 
                                          (controller.connected ? qsTr("已连接到服务器，等待下载命令") : qsTr("未连接到服务器"))
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    // 4. 打开下载文件目录按钮
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: openFolderLayout.implicitHeight + ScreenTools.defaultFontPixelHeight
                        color: qgcPal.windowShade
                        radius: 3

                        ColumnLayout {
                            id: openFolderLayout
                            anchors.fill: parent
                            anchors.margins: ScreenTools.defaultFontPixelWidth
                            spacing: ScreenTools.defaultFontPixelHeight / 2

                            QGCLabel {
                                Layout.fillWidth: true
                                text: qsTr("下载目录")
                                font.bold: true
                                font.pixelSize: ScreenTools.defaultFontPixelSize * 1.1
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    text: controller.selectedFolder
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }

                                QGCButton {
                                    text: qsTr("打开目录")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                    // 注意：此功能需要在后端实现，目前仅作为UI占位
                                    onClicked: {
                                        // 这里需要调用后端方法打开目录，但QGC可能不支持此功能
                                        console.log("打开目录:", controller.selectedFolder)
                                    }
                                }
                            }
                        }
                    }

                    // 5. 调试信息区域
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: debugInfoLayout.implicitHeight + ScreenTools.defaultFontPixelHeight
                        color: qgcPal.windowShade
                        radius: 3

                        ColumnLayout {
                            id: debugInfoLayout
                            anchors.fill: parent
                            anchors.margins: ScreenTools.defaultFontPixelWidth
                            spacing: ScreenTools.defaultFontPixelHeight / 2

                            QGCLabel {
                                Layout.fillWidth: true
                                text: qsTr("调试信息")
                                font.bold: true
                                font.pixelSize: ScreenTools.defaultFontPixelSize * 1.1
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 6
                                color: qgcPal.window
                                border.color: qgcPal.text
                                border.width: 1

                                QGCFlickable {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    contentWidth: width
                                    contentHeight: debugText.height
                                    flickableDirection: Flickable.VerticalFlick
                                    clip: true

                                    QGCLabel {
                                        id: debugText
                                        width: parent.width
                                        wrapMode: Text.WordWrap
                                        text: controller.errorMessage ? 
                                              controller.errorMessage : 
                                              qsTr("连接状态: ") + (controller.connected ? qsTr("已连接") : qsTr("未连接")) + "\n" +
                                              qsTr("服务器地址: ") + controller.serverAddress + ":" + controller.serverPort + "\n" +
                                              qsTr("保存目录: ") + controller.selectedFolder
                                    }
                                }
                            }
                        }
                    }

                    // 错误信息显示
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: errorText.text ? errorText.height + ScreenTools.defaultFontPixelHeight : 0
                        color: qgcPal.warningText
                        visible: errorText.text
                        radius: 3

                        QGCLabel {
                            id: errorText
                            anchors.centerIn: parent
                            width: parent.width - ScreenTools.defaultFontPixelWidth * 2
                            wrapMode: Text.WordWrap
                            text: controller.errorMessage
                            color: "white"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: controller.clearError()
                        }
                    }
                }
            }
        }
    }
} 