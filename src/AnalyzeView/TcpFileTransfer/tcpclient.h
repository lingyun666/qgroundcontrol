#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QRegExp>

// 简化Android特定代码，移除可能不可用的头文件引用
#ifdef Q_OS_ANDROID
// 不直接引用QAndroidApplication，改为在源文件中处理
#endif

/**
 * @brief TCP客户端类，用于下载文件
 */
class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    /**
     * @brief 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @return 是否开始连接成功
     */
    bool connectToServer(const QString &host, quint16 port = 8000);

    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromServer();

    /**
     * @brief 获取服务器文件列表
     */
    void requestFileList();

    /**
     * @brief 下载文件
     * @param fileName 文件名
     * @param savePath 保存路径
     * @return 是否开始下载成功
     */
    bool downloadFile(const QString &fileName, const QString &savePath);

    /**
     * @brief 获取当前连接状态
     * @return 是否已连接
     */
    bool isConnected() const;

signals:
    /**
     * @brief 连接状态变化信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);

    /**
     * @brief 接收到文件列表信号
     * @param fileList 文件列表（文件名和大小的对）
     */
    void fileListReceived(const QList<QPair<QString, qint64>> &fileList);

    /**
     * @brief 下载进度信号
     * @param fileName 文件名
     * @param progress 进度百分比
     */
    void downloadProgress(const QString &fileName, int progress);

    /**
     * @brief 文件接收完成信号
     * @param fileName 文件名
     * @param size 文件大小
     */
    void fileReceived(const QString &fileName, qint64 size);

    /**
     * @brief 错误信息信号
     * @param msg 错误消息
     */
    void error(const QString &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onError(QAbstractSocket::SocketError socketError);

private:
    /**
     * @brief 处理收到的文件头信息
     * @param headerData 文件头数据
     */
    void processFileHeader(const QByteArray &headerData);

    /**
     * @brief 处理收到的文件列表数据
     * @param listData 文件列表数据
     */
    void processFileList(const QByteArray &listData);
    
    /**
     * @brief 手动解析文件列表数据（应对非标准JSON格式）
     * @param data 原始数据
     * @return 解析出的文件列表
     */
    QList<QPair<QString, qint64>> parseFileListManually(const QByteArray &data);
    
    /**
     * @brief 完成文件下载，关闭文件并清理资源
     */
    void finishFileDownload();
    
#ifdef Q_OS_ANDROID
    /**
     * @brief 请求Android存储权限
     * @return 是否已获取所有权限
     */
    bool requestStoragePermissions();
#endif

    QTcpSocket *m_socket;
    QFile *m_file;
    QString m_currentFileName;
    qint64 m_fileSize;
    qint64 m_bytesReceived;
    bool m_receivingHeader;
    bool m_receivingFileList;
    QByteArray m_headerBuffer;
    QByteArray m_fileListBuffer;

    // 命令和消息常量
    static const QByteArray CMD_LIST;  // 请求文件列表命令
    static const QByteArray CMD_GET;   // 请求下载文件命令
    static const int HEADER_SIZE = 1024;
}; 