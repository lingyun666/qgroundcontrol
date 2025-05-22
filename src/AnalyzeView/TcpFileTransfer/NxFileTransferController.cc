#include "NxFileTransferController.h"
#include "tcpclient.h"
#include "QGCApplication.h"
#include "QGCCorePlugin.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QtQml>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>

void NxFileTransferController::registerQmlType()
{
    qmlRegisterType<NxFileTransferController>("QGroundControl.Controllers", 1, 0, "NxFileTransferController");
}

NxFileTransferController::NxFileTransferController(QObject* parent)
    : QObject(parent)
    , m_tcpClient(new TcpClient(this))
    , m_isDownloading(false)
    , m_statusMessage(tr("断开连接"))
    , m_progress(0)
    , m_debugMessage("")
{
    // 初始化下载目录
    _initDownloadDirectory();
    
    // 更新已下载文件列表
    _updateDownloadedFilesList();
    
    // 连接TCP客户端信号与此控制器的槽
    connect(m_tcpClient, &TcpClient::connectionStateChanged, this, &NxFileTransferController::_onConnectionStateChanged);
    connect(m_tcpClient, &TcpClient::fileListReceived, this, &NxFileTransferController::_onFileListReceived);
    connect(m_tcpClient, &TcpClient::downloadProgress, this, &NxFileTransferController::_onDownloadProgress);
    connect(m_tcpClient, &TcpClient::fileReceived, this, &NxFileTransferController::_onFileReceived);
    connect(m_tcpClient, &TcpClient::error, this, &NxFileTransferController::_onError);
}

NxFileTransferController::~NxFileTransferController()
{
    if (m_tcpClient->isConnected()) {
        m_tcpClient->disconnectFromServer();
    }
}

void NxFileTransferController::_initDownloadDirectory()
{
    // 使用应用程序的应用数据目录，这通常不需要特殊权限
#ifdef Q_OS_ANDROID
    m_downloadDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/downloads";
#else
    // 在Windows上使用更友好的目录
    QString downloadLocation = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadLocation.isEmpty()) {
        downloadLocation = QDir::homePath();
    }
    m_downloadDir = downloadLocation + "/QGCFileTransfer";
#endif
    
    // 确保目录路径格式正确
    m_downloadDir = QDir::toNativeSeparators(m_downloadDir);
    
    // 确保目录存在
    QDir dir(m_downloadDir);
    if (!dir.exists()) {
        bool success = dir.mkpath(".");
        if (!success) {
            _addDebugMessage(tr("警告: 无法创建下载目录: %1，将尝试使用临时目录").arg(m_downloadDir));
            // 尝试使用临时目录作为备选
            m_downloadDir = QDir::toNativeSeparators(QDir::tempPath() + "/QGCFileTransfer");
            dir = QDir(m_downloadDir);
            dir.mkpath(".");
        }
    }
    
    // 测试目录是否可写
    QFile testFile(m_downloadDir + "/test.tmp");
    if (testFile.open(QIODevice::WriteOnly)) {
        testFile.close();
        testFile.remove();
    } else {
        _addDebugMessage(tr("警告: 下载目录不可写: %1，将尝试使用临时目录").arg(m_downloadDir));
        // 如果目录不可写，使用临时目录
        m_downloadDir = QDir::toNativeSeparators(QDir::tempPath() + "/QGCFileTransfer");
        dir = QDir(m_downloadDir);
        dir.mkpath(".");
    }
    
    _addDebugMessage(tr("下载目录设置为: %1").arg(m_downloadDir));
}

void NxFileTransferController::_updateDownloadedFilesList()
{
    m_downloadedFiles.clear();
    
    QDir dir(m_downloadDir);
    QStringList filters;
    filters << "*.*";  // 所有文件
    dir.setNameFilters(filters);
    
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Time);
    for (const QFileInfo &fileInfo : fileInfoList) {
        QString fileSize;
        qint64 size = fileInfo.size();
        
        if (size < 1024) {
            fileSize = QString("%1 B").arg(size);
        } else if (size < 1024 * 1024) {
            fileSize = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
        } else if (size < 1024 * 1024 * 1024) {
            fileSize = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
        } else {
            fileSize = QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
        }
        
        m_downloadedFiles.append(QString("%1 (%2)").arg(fileInfo.fileName()).arg(fileSize));
    }
    
    emit downloadedFilesChanged();
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

void NxFileTransferController::downloadFile(const QString& fileName)
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
    
    _updateStatusMessage(tr("开始下载 %1...").arg(fileName));
    m_isDownloading = true;
    emit downloadStatusChanged(true);
    
    if (!m_tcpClient->downloadFile(fileName, m_downloadDir)) {
        m_isDownloading = false;
        emit downloadStatusChanged(false);
        _updateStatusMessage(tr("下载失败"));
    }
}

void NxFileTransferController::openDownloadFolder()
{
    if (!QDir(m_downloadDir).exists()) {
        _updateStatusMessage(tr("下载目录不存在"));
        return;
    }
    
#ifdef Q_OS_ANDROID
    // Android平台特殊处理
    _updateStatusMessage(tr("文件已保存至: %1").arg(m_downloadDir));
    emit errorMessage(tr("Android平台无法直接打开文件目录，请使用文件管理器访问应用程序数据文件夹或使用其他方式查看已下载文件"));
#else
    // 桌面平台尝试打开文件夹
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadDir));
    
    if (!success) {
        _updateStatusMessage(tr("无法打开下载目录: %1").arg(m_downloadDir));
        emit errorMessage(tr("无法打开下载目录，请手动查看路径: %1").arg(m_downloadDir));
    } else {
        _updateStatusMessage(tr("已打开下载目录"));
    }
#endif
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
    
    // 更新已下载文件列表
    _updateDownloadedFilesList();
    
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
    _addDebugMessage(tr("错误: %1").arg(errorMsg));
    
    emit errorMessage(errorMsg);
}

void NxFileTransferController::_updateStatusMessage(const QString& message)
{
    m_statusMessage = message;
    emit statusMessageChanged(message);
    qDebug() << "NxFileTransfer:" << message;
    _addDebugMessage(message);
}

void NxFileTransferController::_addDebugMessage(const QString& message)
{
    // 添加时间戳
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz] ");
    m_debugMessage += timestamp + message + "\n";
    
    // 限制调试消息长度
    if (m_debugMessage.length() > 10000) {
        m_debugMessage = m_debugMessage.right(8000);
    }
    
    emit debugMessageChanged();
}

QString NxFileTransferController::debugMessage() const
{
    return m_debugMessage;
} 