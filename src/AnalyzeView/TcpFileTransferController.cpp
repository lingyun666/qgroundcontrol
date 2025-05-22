#include "TcpFileTransferController.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QApplication>

// 静态常量定义
const QString TcpFileTransferController::DEFAULT_SERVER_ADDRESS = "192.168.1.10";

// 文件信息类实现
FileInfo::FileInfo(const QString &name, qint64 size, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_size(size)
    , m_selected(false)
    , m_status("就绪")
    , m_progress(0)
{
}

QString FileInfo::sizeStr() const
{
    if (m_size < 1024) {
        return QString("%1 B").arg(m_size);
    } else if (m_size < 1024 * 1024) {
        return QString("%1 KB").arg(m_size / 1024.0, 0, 'f', 2);
    } else if (m_size < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(m_size / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(m_size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

void FileInfo::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        emit selectedChanged(selected);
    }
}

void FileInfo::setStatus(const QString &status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged(status);
    }
}

void FileInfo::setProgress(int progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged(progress);
    }
}

// TCP文件传输控制器实现
TcpFileTransferController::TcpFileTransferController(QObject *parent)
    : QObject(parent)
    , m_client(new TcpFileClient(this))
    , m_serverAddress(DEFAULT_SERVER_ADDRESS)
    , m_serverPort(DEFAULT_SERVER_PORT)
    , m_selectedFolder(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
    , m_downloading(false)
{
    // 连接信号和槽
    QObject::connect(m_client, &TcpFileClient::connectionStateChanged, this, &TcpFileTransferController::onConnectionStateChanged);
    QObject::connect(m_client, &TcpFileClient::fileListReceived, this, &TcpFileTransferController::onFileListReceived);
    QObject::connect(m_client, &TcpFileClient::downloadProgress, this, &TcpFileTransferController::onDownloadProgress);
    QObject::connect(m_client, &TcpFileClient::fileReceived, this, &TcpFileTransferController::onFileReceived);
    QObject::connect(m_client, &TcpFileClient::error, this, &TcpFileTransferController::onError);

    qDebug() << "TCP文件传输控制器初始化完成, 默认下载目录:" << m_selectedFolder;
}

TcpFileTransferController::~TcpFileTransferController()
{
    disconnect();
    clearFiles();
}

bool TcpFileTransferController::connected() const
{
    return m_client->isConnected();
}

void TcpFileTransferController::setServerAddress(const QString &address)
{
    if (m_serverAddress != address) {
        m_serverAddress = address;
        emit serverAddressChanged(address);
    }
}

void TcpFileTransferController::setServerPort(int port)
{
    if (m_serverPort != port) {
        m_serverPort = port;
        emit serverPortChanged(port);
    }
}

QStringList TcpFileTransferController::downloadFolders() const
{
    QStringList folders;
    
    // 添加常用路径
    folders << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    folders << QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    folders << QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    
    // 添加应用程序目录
    QString appDir = QApplication::applicationDirPath() + "/downloads";
    QDir dir(appDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    folders << appDir;
    
    return folders;
}

void TcpFileTransferController::setSelectedFolder(const QString &folder)
{
    if (m_selectedFolder != folder) {
        m_selectedFolder = folder;
        emit selectedFolderChanged(folder);
    }
}

void TcpFileTransferController::connect()
{
    qDebug() << "尝试连接到服务器:" << m_serverAddress << ":" << m_serverPort;
    clearError();
    m_client->connectToServer(m_serverAddress, m_serverPort);
}

void TcpFileTransferController::disconnect()
{
    qDebug() << "断开与服务器的连接";
    m_client->disconnectFromServer();
}

void TcpFileTransferController::refresh()
{
    qDebug() << "刷新文件列表";
    clearError();
    
    if (!m_client->isConnected()) {
        setErrorMessage("未连接到服务器，请先连接");
        return;
    }
    
    clearFiles();
    m_client->requestFileList();
}

void TcpFileTransferController::download()
{
    qDebug() << "开始下载文件";
    clearError();
    
    if (!m_client->isConnected()) {
        setErrorMessage("未连接到服务器，请先连接");
        return;
    }
    
    if (m_downloading) {
        setErrorMessage("已有下载任务在进行中");
        return;
    }
    
    // 检查是否有选中的文件
    bool hasSelected = false;
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo && fileInfo->selected()) {
            hasSelected = true;
            break;
        }
    }
    
    if (!hasSelected) {
        setErrorMessage("请至少选择一个文件下载");
        return;
    }
    
    // 开始下载第一个选中的文件
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo && fileInfo->selected()) {
            fileInfo->setStatus("准备下载");
            fileInfo->setProgress(0);
            
            if (m_client->downloadFile(fileInfo->name(), m_selectedFolder)) {
                m_downloading = true;
                emit downloadingChanged(true);
                fileInfo->setStatus("下载中");
                break;
            } else {
                fileInfo->setStatus("下载失败");
            }
        }
    }
}

void TcpFileTransferController::clearSelection()
{
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo) {
            fileInfo->setSelected(false);
        }
    }
}

void TcpFileTransferController::selectAll()
{
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo) {
            fileInfo->setSelected(true);
        }
    }
}

void TcpFileTransferController::clearError()
{
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorMessageChanged(m_errorMessage);
    }
}

void TcpFileTransferController::onConnectionStateChanged(bool connected)
{
    qDebug() << "连接状态变化:" << (connected ? "已连接" : "已断开");
    emit connectedChanged(connected);
    
    if (connected) {
        // 自动刷新文件列表
        refresh();
    } else {
        // 清空文件列表
        clearFiles();
        
        // 如果正在下载，更新状态
        if (m_downloading) {
            m_downloading = false;
            emit downloadingChanged(false);
        }
    }
}

void TcpFileTransferController::onFileListReceived(const QList<QPair<QString, qint64>> &fileList)
{
    qDebug() << "收到文件列表，文件数量:" << fileList.size();
    
    clearFiles();
    
    for (const auto &pair : fileList) {
        FileInfo *fileInfo = new FileInfo(pair.first, pair.second, this);
        m_files.append(fileInfo);
    }
    
    emit filesChanged();
}

void TcpFileTransferController::onDownloadProgress(const QString &fileName, int progress)
{
    FileInfo *fileInfo = findFileByName(fileName);
    if (fileInfo) {
        fileInfo->setProgress(progress);
        fileInfo->setStatus(QString("下载中 %1%").arg(progress));
    }
}

void TcpFileTransferController::onFileReceived(const QString &fileName, qint64 size)
{
    qDebug() << "文件下载完成:" << fileName << ", 大小:" << size;
    
    FileInfo *fileInfo = findFileByName(fileName);
    if (fileInfo) {
        fileInfo->setProgress(100);
        fileInfo->setStatus("下载完成");
        fileInfo->setSelected(false);
    }
    
    // 检查是否还有其他待下载的文件
    for (QObject *obj : m_files) {
        FileInfo *info = qobject_cast<FileInfo*>(obj);
        if (info && info->selected()) {
            info->setStatus("准备下载");
            info->setProgress(0);
            
            if (m_client->downloadFile(info->name(), m_selectedFolder)) {
                info->setStatus("下载中");
                return;
            } else {
                info->setStatus("下载失败");
            }
        }
    }
    
    // 所有文件都已下载完成
    m_downloading = false;
    emit downloadingChanged(false);
}

void TcpFileTransferController::onError(const QString &msg)
{
    setErrorMessage(msg);
    
    // 如果发生错误且正在下载，则恢复状态
    if (m_downloading) {
        // 查找状态为"下载中"的文件，将其状态设为"下载失败"
        for (QObject *obj : m_files) {
            FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
            if (fileInfo && fileInfo->status().startsWith("下载中")) {
                fileInfo->setStatus("下载失败");
                break;
            }
        }
        
        // 继续下载下一个文件
        for (QObject *obj : m_files) {
            FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
            if (fileInfo && fileInfo->selected()) {
                fileInfo->setStatus("准备下载");
                fileInfo->setProgress(0);
                
                if (m_client->downloadFile(fileInfo->name(), m_selectedFolder)) {
                    fileInfo->setStatus("下载中");
                    return;
                } else {
                    fileInfo->setStatus("下载失败");
                }
            }
        }
        
        // 如果没有更多文件可下载，恢复状态
        m_downloading = false;
        emit downloadingChanged(false);
    }
}

void TcpFileTransferController::clearFiles()
{
    for (QObject *obj : m_files) {
        delete obj;
    }
    m_files.clear();
    emit filesChanged();
}

void TcpFileTransferController::setErrorMessage(const QString &message)
{
    qDebug() << "错误:" << message;
    m_errorMessage = message;
    emit errorMessageChanged(message);
}

FileInfo* TcpFileTransferController::findFileByName(const QString &fileName)
{
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo && fileInfo->name() == fileName) {
            return fileInfo;
        }
    }
    return nullptr;
} 