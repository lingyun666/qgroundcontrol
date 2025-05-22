#include "TcpFileTransferController.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

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
    , m_statusMessage("未连接")
    , m_currentDownloadFile("")
    , m_overallProgress(0)
    , m_totalFilesToDownload(0)
    , m_filesDownloaded(0)
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
    setStatusMessage("正在连接...");
    m_client->connectToServer(m_serverAddress, m_serverPort);
}

void TcpFileTransferController::disconnect()
{
    qDebug() << "断开与服务器的连接";
    m_client->disconnectFromServer();
    setStatusMessage("已断开连接");
}

void TcpFileTransferController::refresh()
{
    qDebug() << "刷新文件列表";
    clearError();
    
    if (!m_client->isConnected()) {
        setErrorMessage("未连接到服务器，请先连接");
        return;
    }
    
    setStatusMessage("正在获取文件列表...");
    clearFiles();
    
    // 发送请求前先设置状态，防止请求后状态不正确
    m_downloading = false;
    emit downloadingChanged(false);
    
    // 请求文件列表，注意此操作是异步的
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
    
    // 计算要下载的文件数量
    m_totalFilesToDownload = 0;
    m_filesDownloaded = 0;
    
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo && fileInfo->selected()) {
            m_totalFilesToDownload++;
        }
    }
    
    if (m_totalFilesToDownload == 0) {
        setErrorMessage("请至少选择一个文件下载");
        return;
    }
    
    // 设置状态信息
    setStatusMessage(QString("准备下载 %1 个文件...").arg(m_totalFilesToDownload));
    
    // 开始下载第一个选中的文件
    for (QObject *obj : m_files) {
        FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
        if (fileInfo && fileInfo->selected()) {
            fileInfo->setStatus("准备下载");
            fileInfo->setProgress(0);
            
            m_currentDownloadFile = fileInfo->name();
            emit currentDownloadFileChanged(m_currentDownloadFile);
            
            if (m_client->downloadFile(fileInfo->name(), m_selectedFolder)) {
                m_downloading = true;
                emit downloadingChanged(true);
                fileInfo->setStatus("下载中");
                setStatusMessage(QString("正在下载文件: %1").arg(fileInfo->name()));
                break;
            } else {
                fileInfo->setStatus("下载失败");
                setErrorMessage(QString("无法开始下载文件: %1").arg(fileInfo->name()));
            }
        }
    }
    
    updateOverallProgress();
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
    setErrorMessage("");
}

bool TcpFileTransferController::openDownloadFolder()
{
    QDir dir(m_selectedFolder);
    if (!dir.exists()) {
        setErrorMessage(QString("目录不存在: %1").arg(m_selectedFolder));
        return false;
    }
    
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(m_selectedFolder));
    if (!success) {
        setErrorMessage("无法打开下载目录");
    }
    
    return success;
}

void TcpFileTransferController::onConnectionStateChanged(bool connected)
{
    qDebug() << "连接状态变化:" << connected;
    emit connectedChanged(connected);
    
    if (connected) {
        setStatusMessage("已连接到服务器");
        // 自动刷新文件列表
        refresh();
    } else {
        setStatusMessage("未连接到服务器");
        m_downloading = false;
        emit downloadingChanged(false);
        clearFiles();
    }
}

void TcpFileTransferController::onFileListReceived(const QList<QPair<QString, qint64>> &fileList)
{
    qDebug() << "收到文件列表, 文件数:" << fileList.size();
    
    clearFiles();
    
    for (const auto &file : fileList) {
        FileInfo *fileInfo = new FileInfo(file.first, file.second, this);
        m_files.append(fileInfo);
    }
    
    emit filesChanged();
    
    if (fileList.isEmpty()) {
        setStatusMessage("服务器上没有可用文件");
    } else {
        setStatusMessage(QString("获取到 %1 个文件").arg(fileList.size()));
    }
    
    // 确保在此处不会断开连接
    qDebug() << "文件列表处理完成，保持连接";
}

void TcpFileTransferController::onDownloadProgress(const QString &fileName, int progress)
{
    qDebug() << "文件下载进度:" << fileName << progress << "%";
    
    FileInfo *fileInfo = findFileByName(fileName);
    if (fileInfo) {
        fileInfo->setProgress(progress);
    }
    
    updateOverallProgress();
}

void TcpFileTransferController::onFileReceived(const QString &fileName, qint64 size)
{
    qDebug() << "文件接收完成:" << fileName << "大小:" << size;
    
    FileInfo *fileInfo = findFileByName(fileName);
    if (fileInfo) {
        fileInfo->setStatus("已完成");
        fileInfo->setProgress(100);
    }
    
    m_filesDownloaded++;
    
    // 尝试下载下一个文件
    bool foundNext = false;
    for (QObject *obj : m_files) {
        FileInfo *nextFile = qobject_cast<FileInfo*>(obj);
        if (nextFile && nextFile->selected() && nextFile->status() == "就绪") {
            nextFile->setStatus("准备下载");
            
            m_currentDownloadFile = nextFile->name();
            emit currentDownloadFileChanged(m_currentDownloadFile);
            
            if (m_client->downloadFile(nextFile->name(), m_selectedFolder)) {
                nextFile->setStatus("下载中");
                setStatusMessage(QString("正在下载文件: %1").arg(nextFile->name()));
                foundNext = true;
                break;
            } else {
                nextFile->setStatus("下载失败");
                setErrorMessage(QString("无法开始下载文件: %1").arg(nextFile->name()));
            }
        }
    }
    
    if (!foundNext) {
        m_downloading = false;
        emit downloadingChanged(false);
        
        if (m_filesDownloaded == m_totalFilesToDownload) {
            setStatusMessage(QString("已完成所有 %1 个文件的下载").arg(m_totalFilesToDownload));
        } else {
            setStatusMessage(QString("下载完成 %1/%2 个文件").arg(m_filesDownloaded).arg(m_totalFilesToDownload));
        }
        
        m_currentDownloadFile = "";
        emit currentDownloadFileChanged(m_currentDownloadFile);
    }
    
    updateOverallProgress();
}

void TcpFileTransferController::onError(const QString &msg)
{
    qDebug() << "错误:" << msg;
    setErrorMessage(msg);
    
    if (m_downloading) {
        // 如果在下载过程中出错，尝试继续下载下一个文件
        FileInfo *currentFile = findFileByName(m_currentDownloadFile);
        if (currentFile) {
            currentFile->setStatus("下载失败");
        }
        
        // 尝试下载下一个文件
        bool foundNext = false;
        for (QObject *obj : m_files) {
            FileInfo *nextFile = qobject_cast<FileInfo*>(obj);
            if (nextFile && nextFile->selected() && nextFile->status() == "就绪") {
                nextFile->setStatus("准备下载");
                
                m_currentDownloadFile = nextFile->name();
                emit currentDownloadFileChanged(m_currentDownloadFile);
                
                if (m_client->downloadFile(nextFile->name(), m_selectedFolder)) {
                    nextFile->setStatus("下载中");
                    setStatusMessage(QString("正在下载文件: %1").arg(nextFile->name()));
                    foundNext = true;
                    break;
                } else {
                    nextFile->setStatus("下载失败");
                }
            }
        }
        
        if (!foundNext) {
            m_downloading = false;
            emit downloadingChanged(false);
            
            setStatusMessage(QString("下载完成 %1/%2 个文件，部分文件下载失败").arg(m_filesDownloaded).arg(m_totalFilesToDownload));
            
            m_currentDownloadFile = "";
            emit currentDownloadFileChanged(m_currentDownloadFile);
        }
        
        updateOverallProgress();
    }
}

void TcpFileTransferController::clearFiles()
{
    for (QObject *obj : m_files) {
        obj->deleteLater();
    }
    
    m_files.clear();
    emit filesChanged();
}

void TcpFileTransferController::setErrorMessage(const QString &message)
{
    if (m_errorMessage != message) {
        m_errorMessage = message;
        emit errorMessageChanged(m_errorMessage);
    }
}

void TcpFileTransferController::setStatusMessage(const QString &message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged(m_statusMessage);
    }
}

void TcpFileTransferController::updateOverallProgress()
{
    if (m_totalFilesToDownload == 0) {
        m_overallProgress = 0;
    } else {
        int totalProgress = 0;
        int fileCount = 0;
        
        for (QObject *obj : m_files) {
            FileInfo *fileInfo = qobject_cast<FileInfo*>(obj);
            if (fileInfo && fileInfo->selected()) {
                totalProgress += fileInfo->progress();
                fileCount++;
            }
        }
        
        if (fileCount > 0) {
            m_overallProgress = totalProgress / fileCount;
        } else {
            m_overallProgress = 0;
        }
    }
    
    emit overallProgressChanged(m_overallProgress);
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