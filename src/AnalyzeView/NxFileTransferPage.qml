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
    pageDescription: qsTr("NX文件传输允许您通过TCP协议从NX设备下载文件。整个页面分为5个功能区：连接设置、可用文件列表、下载进度和已下载文件、下载目录以及调试信息。")

    NxFileTransferController {
        id: nxFileTransferController

        onErrorMessage: (message) => {
            mainWindow.showMessageDialog(qsTr("文件传输错误"), message)
        }

        onFileDownloaded: (fileName, size) => {
            mainWindow.showMessageDialog(qsTr("文件下载完成"), 
                qsTr("文件 %1 已成功下载 (%2 字节)").arg(fileName).arg(size))
        }
    }

    Component {
        id: pageComponent

        ScrollView {
            id: pageScrollView
            width: availableWidth
            height: availableHeight
            contentWidth: availableWidth
            contentHeight: mainColumn.height
            clip: true

            ColumnLayout {
                id: mainColumn
                width: pageScrollView.availableWidth
                spacing: ScreenTools.defaultFontPixelHeight

                // 1. 连接设置区域
                QGCGroupBox {
                    Layout.fillWidth: true
                    title: qsTr("连接设置")

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rowSpacing: ScreenTools.defaultFontPixelHeight / 2
                        columnSpacing: ScreenTools.defaultFontPixelWidth

                        QGCLabel { 
                            text: qsTr("设备IP:") 
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        QGCTextField {
                            id: hostField
                            Layout.fillWidth: true
                            text: "192.168.144.100"
                            enabled: !nxFileTransferController.isConnected
                            inputMethodHints: Qt.ImhPreferNumbers
                        }

                        QGCLabel { 
                            text: qsTr("端口:") 
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        QGCTextField {
                            id: portField
                            Layout.fillWidth: true
                            text: "8000"
                            enabled: !nxFileTransferController.isConnected
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: 65535 }
                        }

                        QGCLabel { 
                            text: qsTr("状态:") 
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        QGCLabel {
                            text: nxFileTransferController.statusMessage
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        QGCLabel { 
                            text: qsTr("下载路径:") 
                            Layout.alignment: Qt.AlignRight
                            visible: !ScreenTools.isMobile
                        }
                        
                        QGCLabel {
                            text: nxFileTransferController.downloadPath
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                            visible: !ScreenTools.isMobile
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            height: 1
                            color: qgcPal.text
                            opacity: 0.2
                        }

                        QGCButton {
                            text: nxFileTransferController.isConnected ? qsTr("断开连接") : qsTr("连接")
                            Layout.fillWidth: true
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
                            text: qsTr("刷新文件列表")
                            Layout.fillWidth: true
                            enabled: nxFileTransferController.isConnected && !nxFileTransferController.isDownloading

                            onClicked: {
                                nxFileTransferController.refreshFileList()
                            }
                        }
                    }
                }

                // 2. 可用文件列表
                QGCGroupBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
                    title: qsTr("可用文件列表")

                    ListView {
                        id: fileListView
                        anchors.fill: parent
                        clip: true
                        model: nxFileTransferController.fileList
                        
                        ScrollBar.vertical: ScrollBar {}
                        
                        delegate: Rectangle {
                            width: fileListView.width
                            height: fileItemLayout.height + ScreenTools.defaultFontPixelHeight/2
                            color: index % 2 == 0 ? qgcPal.windowShade : qgcPal.window
                            
                            RowLayout {
                                id: fileItemLayout
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: ScreenTools.defaultFontPixelWidth/2
                                spacing: ScreenTools.defaultFontPixelWidth

                                QGCLabel { 
                                    Layout.fillWidth: true
                                    text: modelData
                                    elide: Text.ElideMiddle
                                }

                                QGCButton {
                                    text: qsTr("下载")
                                    enabled: nxFileTransferController.isConnected && !nxFileTransferController.isDownloading
                                    onClicked: {
                                        // 从显示文本中提取文件名（不包含大小信息）
                                        var fileName = modelData.split(" (")[0]
                                        nxFileTransferController.downloadFile(fileName)
                                    }
                                }
                            }
                        }

                        QGCLabel {
                            anchors.centerIn: parent
                            visible: fileListView.count === 0
                            text: nxFileTransferController.isConnected ? 
                                  qsTr("服务器上没有可用文件") : 
                                  qsTr("请连接到服务器获取文件列表")
                        }
                    }
                }

                // 3. 下载进度和下载好的文件
                QGCGroupBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
                    title: qsTr("下载进度和已下载文件")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: ScreenTools.defaultFontPixelHeight/2

                        // 下载进度显示
                        RowLayout {
                            Layout.fillWidth: true
                            visible: nxFileTransferController.isDownloading
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                text: qsTr("下载进度:")
                            }

                            ProgressBar {
                                id: progressBar
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: nxFileTransferController.progress
                            }

                            QGCLabel {
                                text: nxFileTransferController.progress + "%"
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 5
                            }

                            QGCButton {
                                text: qsTr("取消")
                                onClicked: nxFileTransferController.cancelDownload()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: qgcPal.text
                            opacity: 0.2
                            visible: nxFileTransferController.isDownloading
                        }

                        // 已下载文件列表
                        ListView {
                            id: downloadedFilesView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: nxFileTransferController.downloadedFiles
                            
                            ScrollBar.vertical: ScrollBar {}
                            
                            delegate: Rectangle {
                                width: downloadedFilesView.width
                                height: downloadItemLayout.height + ScreenTools.defaultFontPixelHeight/2
                                color: index % 2 == 0 ? qgcPal.windowShade : qgcPal.window
                                
                                RowLayout {
                                    id: downloadItemLayout
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: ScreenTools.defaultFontPixelWidth/2
                                    spacing: ScreenTools.defaultFontPixelWidth

                                    QGCLabel { 
                                        Layout.fillWidth: true
                                        text: modelData
                                        elide: Text.ElideMiddle
                                    }

                                    QGCButton {
                                        text: qsTr("删除")
                                        onClicked: {
                                            nxFileTransferController.deleteDownloadedFile(modelData)
                                        }
                                    }
                                }
                            }

                            QGCLabel {
                                anchors.centerIn: parent
                                visible: downloadedFilesView.count === 0
                                text: qsTr("没有已下载的文件")
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: downloadedFilesView.count > 0
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCButton {
                                text: qsTr("清空下载列表")
                                Layout.fillWidth: true
                                onClicked: {
                                    mainWindow.showMessageDialog(
                                        qsTr("确认删除"),
                                        qsTr("确定要删除所有已下载的文件吗？"),
                                        Dialog.Yes | Dialog.No,
                                        function() { nxFileTransferController.clearDownloadedFiles() }
                                    )
                                }
                            }
                        }
                    }
                }

                // 4. 打开下载文件的目录按钮
                QGCGroupBox {
                    Layout.fillWidth: true
                    title: qsTr("下载目录")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: ScreenTools.defaultFontPixelHeight/2

                        QGCButton {
                            text: qsTr("打开下载目录")
                            Layout.fillWidth: true
                            onClicked: nxFileTransferController.openDownloadFolder()
                        }

                        QGCLabel {
                            text: qsTr("默认下载路径: ") + nxFileTransferController.downloadPath
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                    }
                }

                // 5. 调试信息
                QGCGroupBox {
                    Layout.fillWidth: true
                    title: qsTr("调试信息")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: ScreenTools.defaultFontPixelHeight/2

                        QGCLabel {
                            text: qsTr("连接状态: ") + (nxFileTransferController.isConnected ? qsTr("已连接") : qsTr("未连接"))
                            Layout.fillWidth: true
                        }

                        QGCLabel {
                            text: qsTr("下载状态: ") + (nxFileTransferController.isDownloading ? qsTr("下载中") : qsTr("空闲"))
                            Layout.fillWidth: true
                        }

                        QGCLabel {
                            text: qsTr("服务器文件数量: ") + nxFileTransferController.fileList.length
                            Layout.fillWidth: true
                        }

                        QGCLabel {
                            text: qsTr("已下载文件数量: ") + nxFileTransferController.downloadedFiles.length
                            Layout.fillWidth: true
                        }

                        QGCLabel {
                            text: qsTr("状态信息: ") + nxFileTransferController.statusMessage
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
} 
