#include "NxFileTransferController.h"
#include "TcpClient.h"
#include "QGCApplication.h"
#include "QGCCorePlugin.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

NxFileTransferController::NxFileTransferController(QObject* parent)
    : QObject(parent)
    , m_tcpClient(new TcpClient(this))
    , m_isDownloading(false)
    , m_statusMessage(tr("断开连接"))
    , m_progress(0)
{
    // 连接TCP客户端信号与此控制器的槽
    connect(m_tcpClient, &TcpClient::connectionStateChanged, this, &NxFileTransferController::_onConnectionStateChanged);
    connect(m_tcpClient, &TcpClient::fileListReceived, this, &NxFileTransferController::_onFileListReceived);
    connect(m_tcpClient, &TcpClient::downloadProgress, this, &NxFileTransferController::_onDownloadProgress);
    connect(m_tcpClient, &TcpClient::fileReceived, this, &NxFileTransferController::_onFileReceived);
    connect(m_tcpClient, &TcpClient::error, this, &NxFileTransferController::_onError);
    
    // 初始化下载路径
    _updateDownloadPath();
}

NxFileTransferController::~NxFileTransferController()
{
    if (m_tcpClient->isConnected()) {
        m_tcpClient->disconnectFromServer();
    }
}

void NxFileTransferController::connectToServer(const QString& host, int port)
{
    _updateStatusMessage(tr("正在连接到 %1:%2...").arg(host).arg(port));
    
    if (!m_tcpClient->connectToServer(host, static_cast<quint16>(port))) {
        _updateStatusMessage(tr("连接失败"));
        emit errorMessage(tr("无法连接到服务器"));
    }
}

void NxFileTransferController::disconnectFromServer()
{
    if (m_tcpClient->isConnected()) {
        m_tcpClient->disconnectFromServer();
        _updateStatusMessage(tr("已断开连接"));
    }
}

void NxFileTransferController::refreshFileList()
{
    if (!m_tcpClient->isConnected()) {
        _updateStatusMessage(tr("未连接到服务器"));
        emit errorMessage(tr("请先连接到服务器"));
        return;
    }
    
    _updateStatusMessage(tr("正在获取文件列表..."));
    m_tcpClient->requestFileList();
}

void NxFileTransferController::downloadFile(const QString& fileName, const QString& savePath)
{
    if (!m_tcpClient->isConnected()) {
        _updateStatusMessage(tr("未连接到服务器"));
        emit errorMessage(tr("请先连接到服务器"));
        return;
    }
    
    if (m_isDownloading) {
        _updateStatusMessage(tr("已有文件正在下载中"));
        emit errorMessage(tr("请等待当前文件下载完成"));
        return;
    }
    
    QString path = savePath;
    if (path.isEmpty()) {
#ifdef Q_OS_ANDROID
        // 在Android上使用指定的下载路径
        path = "/storage/emulated/0/Android/data/org.mavlink.qgroundcontrol/files/Download";
#else
        // 在其他平台上使用默认下载位置
        path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#endif
    }
    
    // 确保下载目录存在
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        _updateStatusMessage(tr("无法创建下载目录"));
        emit errorMessage(tr("无法创建目录: %1").arg(path));
        return;
    }
    
    _updateStatusMessage(tr("开始下载 %1...").arg(fileName));
    m_isDownloading = true;
    emit downloadStatusChanged(true);
    
    if (!m_tcpClient->downloadFile(fileName, path)) {
        m_isDownloading = false;
        emit downloadStatusChanged(false);
        _updateStatusMessage(tr("下载失败"));
    }
}

void NxFileTransferController::cancelDownload()
{
    if (m_isDownloading) {
        disconnectFromServer();
        m_isDownloading = false;
        emit downloadStatusChanged(false);
        _updateStatusMessage(tr("下载已取消"));
    }
}

void NxFileTransferController::openDownloadFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadPath));
}

void NxFileTransferController::deleteDownloadedFile(const QString& fileName)
{
    QString filePath = m_downloadPath + "/" + fileName;
    QFile file(filePath);
    
    if (file.exists()) {
        if (file.remove()) {
            // 从下载列表中移除
            m_downloadedFiles.removeOne(fileName);
            emit downloadedFilesChanged();
            _updateStatusMessage(tr("已删除文件: %1").arg(fileName));
        } else {
            emit errorMessage(tr("无法删除文件: %1").arg(fileName));
        }
    }
}

void NxFileTransferController::clearDownloadedFiles()
{
    bool success = true;
    
    for (const QString& fileName : m_downloadedFiles) {
        QString filePath = m_downloadPath + "/" + fileName;
        QFile file(filePath);
        
        if (file.exists() && !file.remove()) {
            success = false;
        }
    }
    
    if (success) {
        m_downloadedFiles.clear();
        emit downloadedFilesChanged();
        _updateStatusMessage(tr("已清空下载列表"));
    } else {
        emit errorMessage(tr("部分文件无法删除"));
    }
}

bool NxFileTransferController::isConnected() const
{
    return m_tcpClient->isConnected();
}

QStringList NxFileTransferController::fileList() const
{
    QStringList files;
    for (const auto& file : m_fileList) {
        // 格式化为: 文件名 (大小)
        QString sizeStr;
        qint64 size = file.second;
        
        if (size < 1024) {
            sizeStr = QString("%1 B").arg(size);
        } else if (size < 1024 * 1024) {
            sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
        } else if (size < 1024 * 1024 * 1024) {
            sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
        } else {
            sizeStr = QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
        }
        
        files.append(QString("%1 (%2)").arg(file.first).arg(sizeStr));
    }
    return files;
}

void NxFileTransferController::_updateDownloadPath()
{
#ifdef Q_OS_ANDROID
    // 在Android上使用指定的下载路径
    m_downloadPath = "/storage/emulated/0/Android/data/org.mavlink.qgroundcontrol/files/Download";
#else
    // 在其他平台上使用默认下载位置
    m_downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#endif

    // 确保目录存在
    QDir dir(m_downloadPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 扫描已下载的文件
    m_downloadedFiles.clear();
    QStringList nameFilters;
    nameFilters << "*.*";
    QFileInfoList fileInfoList = dir.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Time);
    
    for (const QFileInfo& fileInfo : fileInfoList) {
        m_downloadedFiles.append(fileInfo.fileName());
    }
    
    emit downloadPathChanged();
    emit downloadedFilesChanged();
}

void NxFileTransferController::_onConnectionStateChanged(bool connected)
{
    if (connected) {
        _updateStatusMessage(tr("已连接到服务器"));
        // 自动请求文件列表
        m_tcpClient->requestFileList();
    } else {
        _updateStatusMessage(tr("已断开连接"));
        if (m_isDownloading) {
            m_isDownloading = false;
            emit downloadStatusChanged(false);
        }
        
        // 清空文件列表
        m_fileList.clear();
        emit fileListChanged();
    }
    
    emit connectionStateChanged(connected);
}

void NxFileTransferController::_onFileListReceived(const QList<QPair<QString, qint64>>& fileList)
{
    m_fileList = fileList;
    
    if (fileList.isEmpty()) {
        _updateStatusMessage(tr("服务器没有可用文件"));
    } else {
        _updateStatusMessage(tr("接收到 %1 个文件").arg(fileList.size()));
    }
    
    emit fileListChanged();
}

void NxFileTransferController::_onDownloadProgress(const QString& fileName, int progress)
{
    m_progress = progress;
    _updateStatusMessage(tr("下载中 %1: %2%").arg(fileName).arg(progress));
    
    emit progressChanged(progress);
}

void NxFileTransferController::_onFileReceived(const QString& fileName, qint64 size)
{
    m_isDownloading = false;
    m_progress = 100;
    
    QString sizeStr;
    if (size < 1024) {
        sizeStr = QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else if (size < 1024 * 1024 * 1024) {
        sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        sizeStr = QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    }
    
    _updateStatusMessage(tr("文件 %1 (%2) 下载完成").arg(fileName).arg(sizeStr));
    
    // 更新下载文件列表
    if (!m_downloadedFiles.contains(fileName)) {
        m_downloadedFiles.append(fileName);
        emit downloadedFilesChanged();
    }
    
    emit downloadStatusChanged(false);
    emit progressChanged(100);
    emit fileDownloaded(fileName, size);
}

void NxFileTransferController::_onError(const QString& errorMsg)
{
    if (m_isDownloading) {
        m_isDownloading = false;
        emit downloadStatusChanged(false);
    }
    
    _updateStatusMessage(tr("错误: %1").arg(errorMsg));
    
    emit errorMessage(errorMsg);
}

void NxFileTransferController::_updateStatusMessage(const QString& message)
{
    m_statusMessage = message;
    emit statusMessageChanged(message);
    qDebug() << "NxFileTransfer:" << message;
} 