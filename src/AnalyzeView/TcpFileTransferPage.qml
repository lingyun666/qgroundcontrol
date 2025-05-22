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

            ColumnLayout {
                anchors.fill: parent
                spacing: ScreenTools.defaultFontPixelHeight / 2

                // 服务器连接部分
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: connectionGrid.height + ScreenTools.defaultFontPixelHeight
                    color: qgcPal.windowShade
                    radius: 3

                    GridLayout {
                        id: connectionGrid
                        anchors.centerIn: parent
                        columns: 4
                        rowSpacing: ScreenTools.defaultFontPixelHeight / 2
                        columnSpacing: ScreenTools.defaultFontPixelWidth

                        QGCLabel { 
                            text: qsTr("服务器地址:") 
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        QGCTextField {
                            id: serverAddressField
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 20
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
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 30
                            model: controller.downloadFolders
                            currentIndex: controller.downloadFolders.indexOf(controller.selectedFolder)
                            onActivated: controller.selectedFolder = currentText
                        }

                        Item { width: 1; height: 1 } // 占位符

                        RowLayout {
                            spacing: ScreenTools.defaultFontPixelWidth

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

                // 文件列表部分
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: ScreenTools.defaultFontPixelWidth

                    QGCFlickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: fileGrid.width
                        contentHeight: fileGrid.height
                        clip: true

                        GridLayout {
                            id: fileGrid
                            width: parent.width
                            columns: 5
                            flow: GridLayout.TopToBottom
                            rowSpacing: 0
                            columnSpacing: ScreenTools.defaultFontPixelWidth

                            // 标题行
                            QGCCheckBox {
                                id: selectAllCheckBox
                                enabled: controller.files.length > 0 && !controller.downloading
                                onClicked: {
                                    if (checked) {
                                        controller.selectAll()
                                    } else {
                                        controller.clearSelection()
                                    }
                                }
                            }

                            QGCLabel { text: qsTr("文件名") }
                            QGCLabel { text: qsTr("大小") }
                            QGCLabel { text: qsTr("状态") }
                            QGCLabel { text: qsTr("进度") }

                            // 文件列表
                            Repeater {
                                model: controller.files

                                QGCCheckBox {
                                    enabled: !controller.downloading
                                    checked: modelData.selected
                                    onClicked: modelData.selected = checked
                                }
                            }

                            Repeater {
                                model: controller.files
                                QGCLabel { text: modelData.name }
                            }

                            Repeater {
                                model: controller.files
                                QGCLabel { text: modelData.sizeStr }
                            }

                            Repeater {
                                model: controller.files
                                QGCLabel { text: modelData.status }
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

                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        spacing: ScreenTools.defaultFontPixelHeight

                        QGCButton {
                            text: qsTr("下载")
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                            enabled: controller.connected && !controller.downloading
                            onClicked: controller.download()
                        }

                        QGCButton {
                            text: qsTr("全选")
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                            enabled: controller.files.length > 0 && !controller.downloading
                            onClicked: {
                                controller.selectAll()
                                selectAllCheckBox.checked = true
                            }
                        }

                        QGCButton {
                            text: qsTr("取消全选")
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                            enabled: controller.files.length > 0 && !controller.downloading
                            onClicked: {
                                controller.clearSelection()
                                selectAllCheckBox.checked = false
                            }
                        }
                    }
                }

                // 错误信息部分
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