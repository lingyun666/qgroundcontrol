#include "TcpClient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDataStream>

#ifdef Q_OS_ANDROID
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QCoreApplication>
#include <QtCore/private/qandroidextras_p.h>
#else
#include <QtAndroid>
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QtAndroidExtras>
#endif
#include <QStandardPaths>
#endif

// 定义命令常量
const QByteArray TcpClient::CMD_LIST = "LIST";
const QByteArray TcpClient::CMD_GET = "GET";

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_file(nullptr)
    , m_fileSize(0)
    , m_bytesReceived(0)
    , m_receivingHeader(false)
    , m_receivingFileList(false)
{
    // 连接信号和槽
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpClient::onBytesWritten);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), 
            this, &TcpClient::onError);
}

TcpClient::~TcpClient()
{
    disconnectFromServer();
}

bool TcpClient::connectToServer(const QString &host, quint16 port)
{
    qDebug() << "尝试连接到服务器:" << host << ", 端口:" << port;
    
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "已经连接到服务器，先断开连接";
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected();
    }
    
    // 连接服务器
    m_socket->connectToHost(host, port);
    return true;
}

void TcpClient::disconnectFromServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        
        // 如果socket状态不是立即断开，则等待
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
    
    // 清理文件资源
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void TcpClient::requestFileList()
{
    qDebug() << "请求服务器文件列表, 当前连接状态:" << (m_socket->state() == QAbstractSocket::ConnectedState ? "已连接" : "未连接");
    
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit error("未连接到服务器");
        return;
    }
    
    // 清空文件列表缓冲区
    m_fileListBuffer.clear();
    m_receivingFileList = true;
    
    // 发送请求文件列表命令
    m_socket->write(CMD_LIST);
}

#ifdef Q_OS_ANDROID
bool TcpClient::requestStoragePermissions()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6中的权限请求方式
    const QStringList permissions = {
        "android.permission.READ_EXTERNAL_STORAGE",
        "android.permission.WRITE_EXTERNAL_STORAGE"
    };

    // 检查每个权限
    for (const QString &permission : permissions) {
        auto result = QtAndroidPrivate::checkPermission(permission).result();
        if (result == QtAndroidPrivate::PermissionResult::Denied) {
            // Qt 6中权限请求方式不同，无法同步请求
            // 在实际应用中应该使用异步方式并等待结果
            // 这里简化处理，只检查权限状态
            qDebug() << "权限被拒绝:" << permission;
            return false;
        }
    }
#else
    // Qt 5中的权限请求方式
    const QStringList permissions = {
        "android.permission.READ_EXTERNAL_STORAGE",
        "android.permission.WRITE_EXTERNAL_STORAGE"
    };

    // 检查和请求每个权限
    for (const QString &permission : permissions) {
        auto result = QtAndroid::checkPermission(permission);
        if (result == QtAndroid::PermissionResult::Denied) {
            // 请求权限
            auto resultHash = QtAndroid::requestPermissionsSync(QStringList({permission}));
            if (resultHash[permission] == QtAndroid::PermissionResult::Denied) {
                qDebug() << "权限被拒绝:" << permission;
                return false;
            }
        }
    }
#endif
    return true;
}
#endif

bool TcpClient::downloadFile(const QString &fileName, const QString &savePath)
{
#ifdef Q_OS_ANDROID
    // 在Android上，确保有存储权限
    if (!requestStoragePermissions()) {
        emit error("没有存储权限，无法下载文件");
        return false;
    }
#endif

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit error("未连接到服务器");
        return false;
    }

    // 检查是否已有下载中的文件
    if (m_file) {
        emit error("已有文件正在下载中");
        return false;
    }

    // 确保保存目录存在
    QFileInfo fileInfo(savePath);
    QDir saveDir = fileInfo.dir();
    
    // 记录详细日志
    qDebug() << "尝试下载文件: " << fileName << " 到路径: " << savePath;
    qDebug() << "目录信息: " << saveDir.path() << ", 存在状态: " << saveDir.exists();
    
    if (!saveDir.exists()) {
        qDebug() << "尝试创建目录: " << saveDir.path();
        if (!saveDir.mkpath(".")) {
            QString errorMsg = QString("无法创建目录: %1").arg(saveDir.path());
            qDebug() << errorMsg;
            emit error(errorMsg);
            return false;
        }
    }

    // 创建完整的保存路径
    QString fullPath = savePath + "/" + fileName;
    qDebug() << "文件将保存至: " << fullPath;
    
    m_file = new QFile(fullPath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        QString errorMsg = QString("无法创建文件: %1 (错误: %2)").arg(fullPath, m_file->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        delete m_file;
        m_file = nullptr;
        return false;
    }

    // 初始化变量
    m_currentFileName = fileName;
    m_fileSize = 0;
    m_bytesReceived = 0;
    m_receivingHeader = true;
    m_headerBuffer.clear();

    // 发送下载文件的命令
    QByteArray command = CMD_GET + " " + fileName.toUtf8();
    qint64 bytesWritten = m_socket->write(command);
    
    if (bytesWritten <= 0) {
        QString errorMsg = QString("发送下载命令失败: %1").arg(m_socket->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        m_file->close();
        delete m_file;
        m_file = nullptr;
        return false;
    }
    
    qDebug() << "发送下载命令: " << command;
    return true;
}

bool TcpClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::onConnected()
{
    qDebug() << "已连接到服务器:" << m_socket->peerAddress().toString() << ":" << m_socket->peerPort();
    emit connectionStateChanged(true);
}

void TcpClient::onDisconnected()
{
    qDebug() << "与服务器断开连接";
    
    // 清理资源
    if (m_file) {
        // 如果正在下载文件但未完成，发出错误信号
        if (m_bytesReceived < m_fileSize) {
            emit error(QString("下载中断: %1 (已接收 %2/%3 字节)")
                       .arg(m_currentFileName)
                       .arg(m_bytesReceived)
                       .arg(m_fileSize));
        }
        
        m_file->close();
        delete m_file;
        m_file = nullptr;
        m_currentFileName.clear();
        m_fileSize = 0;
        m_bytesReceived = 0;
    }
    
    // 清理其它缓冲区
    m_headerBuffer.clear();
    m_fileListBuffer.clear();
    m_receivingHeader = false;
    m_receivingFileList = false;
    
    emit connectionStateChanged(false);
}

void TcpClient::onReadyRead()
{
    // 处理连接断开
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    while (m_socket->bytesAvailable() > 0) {
        // 1. 接收文件头
        if (m_receivingHeader) {
            // 收集足够的数据用于解析文件头
            m_headerBuffer.append(m_socket->read(HEADER_SIZE - m_headerBuffer.size()));
            
            // 检查是否已收集了足够的数据
            if (m_headerBuffer.size() >= HEADER_SIZE) {
                m_receivingHeader = false;
                processFileHeader(m_headerBuffer);
                m_headerBuffer.clear();
            } else {
                // 继续等待更多数据
                break;
            }
        }
        // 2. 接收文件列表
        else if (m_receivingFileList) {
            m_fileListBuffer.append(m_socket->readAll());
            
            // 尝试解析文件列表 (检查JSON格式是否完整)
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(m_fileListBuffer, &parseError);
            
            if (parseError.error == QJsonParseError::NoError) {
                // 成功解析完整的JSON数据
                m_receivingFileList = false;
                processFileList(m_fileListBuffer);
                m_fileListBuffer.clear();
            } else {
                // JSON不完整，等待更多数据
                break;
            }
        }
        // 3. 接收文件数据
        else if (m_file) {
            // 读取所有可用数据
            QByteArray buffer = m_socket->readAll();
            qint64 bytesWritten = m_file->write(buffer);
            
            if (bytesWritten != buffer.size()) {
                emit error(QString("写入文件失败: %1").arg(m_file->errorString()));
                finishFileDownload();
                disconnectFromServer();
                return;
            }
            
            m_bytesReceived += bytesWritten;
            
            // 计算下载进度
            int progress = 0;
            if (m_fileSize > 0) {
                progress = static_cast<int>((m_bytesReceived * 100) / m_fileSize);
            }
            
            emit downloadProgress(m_currentFileName, progress);
            
            // 检查是否下载完成
            if (m_bytesReceived >= m_fileSize) {
                finishFileDownload();
            }
        }
        // 4. 未预期的数据 (清空缓冲区)
        else {
            m_socket->readAll();
            break;
        }
    }
}

void TcpClient::processFileHeader(const QByteArray &headerData)
{
    // 解析文件头
    QDataStream stream(headerData);
    stream.setVersion(QDataStream::Qt_5_12);
    
    qint64 fileSize;
    QString fileName;
    
    stream >> fileSize >> fileName;
    
    qDebug() << "接收到文件头信息: 文件名=" << fileName << ", 大小=" << fileSize << "字节";
    
    // 检查文件名是否匹配
    if (fileName != m_currentFileName) {
        emit error(QString("收到的文件名不匹配: 请求=%1, 收到=%2").arg(m_currentFileName, fileName));
        finishFileDownload();
        return;
    }
    
    // 更新文件大小
    m_fileSize = fileSize;
    
    // 检查文件是否存在内容
    if (fileSize <= 0) {
        emit error(QString("文件 %1 大小无效: %2").arg(fileName).arg(fileSize));
        finishFileDownload();
        return;
    }
    
    // 准备接收文件数据
    m_bytesReceived = 0;
    emit downloadProgress(m_currentFileName, 0);
}

void TcpClient::processFileList(const QByteArray &listData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(listData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit error(QString("文件列表JSON解析失败: %1").arg(parseError.errorString()));
        return;
    }
    
    QJsonArray array = doc.array();
    QList<QPair<QString, qint64>> fileList;
    
    for (int i = 0; i < array.size(); i++) {
        QJsonObject obj = array[i].toObject();
        QString fileName = obj["name"].toString();
        qint64 fileSize = obj["size"].toVariant().toLongLong();
        
        fileList.append(qMakePair(fileName, fileSize));
    }
    
    qDebug() << "收到文件列表，包含" << fileList.size() << "个文件";
    emit fileListReceived(fileList);
}

void TcpClient::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
}

void TcpClient::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage;
    
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMessage = "连接被拒绝，请确认服务器是否运行";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorMessage = "远程主机关闭了连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMessage = "找不到主机，请检查IP地址";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMessage = "连接超时";
        break;
    case QAbstractSocket::NetworkError:
        errorMessage = "网络错误，请检查网络连接";
        break;
    default:
        errorMessage = QString("套接字错误: %1").arg(m_socket->errorString());
        break;
    }
    
    qDebug() << "TCP客户端错误: " << errorMessage;
    emit error(errorMessage);
}

void TcpClient::finishFileDownload()
{
    if (!m_file) {
        return;
    }
    
    QString fileName = m_currentFileName;
    qint64 fileSize = m_bytesReceived;
    
    m_file->close();
    delete m_file;
    m_file = nullptr;
    
    m_currentFileName.clear();
    m_fileSize = 0;
    m_bytesReceived = 0;
    
    qDebug() << "文件下载完成: " << fileName << ", 大小: " << fileSize << "字节";
    emit fileReceived(fileName, fileSize);
} 