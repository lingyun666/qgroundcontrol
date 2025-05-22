/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Controllers
import QGroundControl.ScreenTools

AnalyzePage {
    id: nxFileTransferPage
    pageComponent: pageComponent
    pageDescription: qsTr("NX文件传输允许您通过TCP协议从NX设备下载文件。")

    NxFileTransferController {
        id: nxFileTransferController

        onErrorMessage: (message) => {
            mainWindow.showMessageDialog(qsTr("文件传输错误"), message)
        }

        onFileDownloaded: (fileName, size) => {
            mainWindow.showMessageDialog(qsTr("文件下载完成"), 
                qsTr("文件 %1 已成功下载 (%2 字节)").arg(fileName).arg(size))
        }
        
        // 监听状态消息以便记录到调试区域
        onStatusMessageChanged: {
            debugTextArea.cursorPosition = debugTextArea.length
        }
    }

    Component {
        id: pageComponent

        Item {
            id: mainItem
            width: availableWidth
            height: availableHeight
            
            // 使用Flickable实现整个页面的滚动
            Flickable {
                id: mainFlickable
                anchors.fill: parent
                contentWidth: mainItem.width
                contentHeight: mainColumn.height
                flickableDirection: Flickable.VerticalFlick
                clip: true
                
                // 添加滚动条
                ScrollBar.vertical: ScrollBar {
                    active: true
                    policy: ScrollBar.AsNeeded
                }

                // 主布局列
                ColumnLayout {
                    id: mainColumn
                    width: parent.width
                    spacing: ScreenTools.defaultFontPixelHeight / 2
                    
                    //-----------------------------
                    // 1. 连接设置区块
                    //-----------------------------
                    QGCGroupBox {
                        Layout.fillWidth: true
                        title: qsTr("连接设置")
                        
                        GridLayout {
                            width: parent.width
                            columns: 4
                            rowSpacing: ScreenTools.defaultFontPixelHeight / 3
                            columnSpacing: ScreenTools.defaultFontPixelWidth
                            
                            QGCLabel {
                                text: qsTr("设备IP:")
                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                            }
                            
                            QGCTextField {
                                id: hostField
                                Layout.fillWidth: true
                                Layout.columnSpan: 3
                                text: "192.168.1.1"
                                enabled: !nxFileTransferController.isConnected
                                inputMethodHints: Qt.ImhPreferNumbers
                            }
                            
                            QGCLabel {
                                text: qsTr("端口:")
                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                            }
                            
                            QGCTextField {
                                id: portField
                                Layout.fillWidth: true
                                text: "8000"
                                enabled: !nxFileTransferController.isConnected
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 1; top: 65535 }
                            }
                            
                            QGCButton {
                                text: nxFileTransferController.isConnected ? qsTr("断开连接") : qsTr("连接")
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.8
                                enabled: hostField.text.length > 0 && portField.text.length > 0 && !nxFileTransferController.isDownloading
                                
                                onClicked: {
                                    if (nxFileTransferController.isConnected) {
                                        nxFileTransferController.disconnectFromServer()
                                    } else {
                                        nxFileTransferController.connectToServer(hostField.text, parseInt(portField.text))
                                    }
                                }
                            }
                            
                            QGCButton {
                                text: qsTr("刷新列表")
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.8
                                enabled: nxFileTransferController.isConnected && !nxFileTransferController.isDownloading
                                onClicked: nxFileTransferController.refreshFileList()
                            }
                            
                            QGCLabel {
                                text: qsTr("状态:")
                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                            }
                            
                            QGCLabel {
                                text: nxFileTransferController.statusMessage
                                Layout.fillWidth: true
                                Layout.columnSpan: 3
                                wrapMode: Text.WordWrap
                                font.bold: true
                                color: nxFileTransferController.isConnected ? "green" : "black"
                            }
                        }
                    }
                    
                    //-----------------------------
                    // 2. 可用文件列表区块
                    //-----------------------------
                    QGCGroupBox {
                        id: fileListGroupBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
                        title: qsTr("服务器文件列表")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0
                            
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 0.8
                                color: qgcPal.windowShade
                                visible: nxFileTransferController.fileList.length > 0
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: ScreenTools.defaultFontPixelWidth
                                    anchors.rightMargin: ScreenTools.defaultFontPixelWidth
                                    
                                    QGCLabel {
                                        Layout.fillWidth: true
                                        text: qsTr("文件名")
                                        font.bold: true
                                    }
                                    
                                    QGCLabel {
                                        text: qsTr("操作")
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                        font.bold: true
                                    }
                                }
                            }
                            
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                
                                // 无文件时显示的提示
                                QGCLabel {
                                    anchors.centerIn: parent
                                    text: qsTr("暂无文件可下载\n请连接设备或点击刷新")
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: nxFileTransferController.fileList.length === 0
                                    color: qgcPal.colorGrey
                                }
                                
                                // 文件列表
                                ListView {
                                    id: fileListView
                                    anchors.fill: parent
                                    clip: true
                                    model: nxFileTransferController.fileList
                                    visible: nxFileTransferController.fileList.length > 0
                                    
                                    // 添加滚动条
                                    ScrollBar.vertical: ScrollBar {
                                        active: true
                                        policy: ScrollBar.AsNeeded
                                        width: ScreenTools.defaultFontPixelWidth
                                    }
                                    
                                    delegate: Rectangle {
                                        width: fileListView.width
                                        height: fileItemLayout.height + ScreenTools.defaultFontPixelHeight / 2
                                        color: index % 2 ? qgcPal.windowShade : "transparent"
                                        
                                        RowLayout {
                                            id: fileItemLayout
                                            width: parent.width - ScreenTools.defaultFontPixelWidth * 2
                                            anchors.centerIn: parent
                                            spacing: ScreenTools.defaultFontPixelWidth
                                            
                                            QGCLabel {
                                                Layout.fillWidth: true
                                                text: modelData
                                                elide: Text.ElideMiddle
                                            }
                                            
                                            QGCButton {
                                                text: qsTr("下载")
                                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                                enabled: nxFileTransferController.isConnected && !nxFileTransferController.isDownloading
                                                onClicked: {
                                                    // 从显示文本中提取文件名（不包含大小信息）
                                                    var fileName = modelData.split(" (")[0]
                                                    nxFileTransferController.downloadFile(fileName)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    //-----------------------------
                    // 3. 下载进度和已下载文件区块
                    //-----------------------------
                    QGCGroupBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
                        title: qsTr("下载进度和已下载文件")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: ScreenTools.defaultFontPixelHeight / 3
                            
                            // 下载进度条
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: ScreenTools.defaultFontPixelHeight / 4
                                visible: nxFileTransferController.isDownloading
                                
                                QGCLabel {
                                    Layout.fillWidth: true
                                    text: qsTr("正在下载...")
                                    font.bold: true
                                }
                                
                                ProgressBar {
                                    id: progressBar
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: nxFileTransferController.progress
                                }
                                
                                QGCButton {
                                    text: qsTr("取消下载")
                                    Layout.alignment: Qt.AlignRight
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 20
                                    onClicked: nxFileTransferController.cancelDownload()
                                }
                            }
                            
                            // 已下载文件标题
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 0.8
                                color: qgcPal.windowShade
                                visible: nxFileTransferController.downloadedFiles.length > 0
                                
                                QGCLabel {
                                    anchors.fill: parent
                                    anchors.leftMargin: ScreenTools.defaultFontPixelWidth
                                    text: qsTr("已下载文件")
                                    font.bold: true
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            
                            // 已下载文件列表
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                
                                // 无已下载文件时显示的提示
                                QGCLabel {
                                    anchors.centerIn: parent
                                    text: qsTr("暂无已下载文件")
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: nxFileTransferController.downloadedFiles.length === 0
                                    color: qgcPal.colorGrey
                                }
                                
                                // 已下载文件列表
                                ListView {
                                    id: downloadedFilesListView
                                    anchors.fill: parent
                                    clip: true
                                    model: nxFileTransferController.downloadedFiles
                                    visible: nxFileTransferController.downloadedFiles.length > 0
                                    
                                    // 添加滚动条
                                    ScrollBar.vertical: ScrollBar {
                                        active: true
                                        policy: ScrollBar.AsNeeded
                                        width: ScreenTools.defaultFontPixelWidth
                                    }
                                    
                                    delegate: Rectangle {
                                        width: downloadedFilesListView.width
                                        height: downloadedFileItemLayout.height + ScreenTools.defaultFontPixelHeight / 2
                                        color: index % 2 ? qgcPal.windowShade : "transparent"
                                        
                                        RowLayout {
                                            id: downloadedFileItemLayout
                                            width: parent.width - ScreenTools.defaultFontPixelWidth * 2
                                            anchors.centerIn: parent
                                            
                                            QGCLabel {
                                                Layout.fillWidth: true
                                                text: modelData
                                                elide: Text.ElideMiddle
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    //-----------------------------
                    // 4. 打开下载文件目录按钮
                    //-----------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: ScreenTools.defaultFontPixelHeight / 4
                        
                        QGCButton {
                            Layout.fillWidth: true
                            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2
                            text: qsTr("打开下载文件目录")
                            onClicked: nxFileTransferController.openDownloadFolder()
                        }
                        
                        QGCLabel {
                            Layout.fillWidth: true
                            text: qsTr("保存位置: ") + nxFileTransferController.downloadDir
                            elide: Text.ElideMiddle
                            wrapMode: Text.Wrap
                            font.pixelSize: ScreenTools.defaultFontPixelSize * 0.8
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    
                    //-----------------------------
                    // 5. 调试信息区块
                    //-----------------------------
                    QGCGroupBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 10
                        title: qsTr("调试信息")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0
                            
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.5
                                
                                QGCButton {
                                    text: qsTr("清除日志")
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 20
                                    onClicked: nxFileTransferController.clearDebugMessages()
                                }
                                
                                Item { Layout.fillWidth: true }
                            }
                            
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: Qt.rgba(0, 0, 0, 0.05)
                                border.width: 1
                                border.color: Qt.rgba(0, 0, 0, 0.1)
                                
                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    
                                    TextArea {
                                        id: debugTextArea
                                        readOnly: true
                                        wrapMode: TextEdit.Wrap
                                        text: nxFileTransferController.debugMessage
                                        font.family: "Courier"
                                        font.pixelSize: ScreenTools.defaultFontPixelSize * 0.8
                                    }
                                }
                            }
                        }
                    }
                    
                    // 底部间距
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
                    }
                }
            }
        }
    }
} 