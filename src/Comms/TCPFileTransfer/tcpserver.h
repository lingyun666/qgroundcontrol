#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QDir>

/**
 * @brief TCP服务端类，用于提供文件下载
 */
class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    /**
     * @brief 启动服务器监听
     * @param port 监听端口
     * @return 是否成功启动
     */
    bool startServer(quint16 port = 8000);

    /**
     * @brief 停止服务器
     */
    void stopServer();

    /**
     * @brief 检查服务器是否正在监听
     * @return 是否正在监听
     */
    bool isListening() const;

    /**
     * @brief 设置共享文件的目录
     * @param dir 共享目录路径
     */
    void setShareDirectory(const QString &dir);

    /**
     * @brief 获取共享目录中的文件列表
     * @return 文件列表（文件名和大小的对）
     */
    QList<QPair<QString, qint64>> getFileList() const;

signals:
    /**
     * @brief 新客户端连接信号
     * @param ip 客户端IP地址
     */
    void newConnection(const QString &ip);

    /**
     * @brief 客户端请求文件信号
     * @param fileName 文件名
     */
    void clientRequest(const QString &fileName);

    /**
     * @brief 发送进度信号
     * @param fileName 文件名
     * @param progress 进度百分比
     */
    void sendProgress(const QString &fileName, int progress);

    /**
     * @brief 文件发送完成信号
     * @param fileName 文件名
     * @param size 文件大小
     */
    void fileSent(const QString &fileName, qint64 size);

    /**
     * @brief 错误信息信号
     * @param msg 错误消息
     */
    void error(const QString &msg);

    /**
     * @brief 服务器状态信号
     * @param running 是否正在运行
     */
    void serverStatus(bool running);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onBytesWritten(qint64 bytes);
    void onError(QAbstractSocket::SocketError socketError);

private:
    /**
     * @brief 发送文件给客户端
     * @param socket 客户端socket
     * @param fileName 文件名
     */
    void sendFile(QTcpSocket *socket, const QString &fileName);

    /**
     * @brief 发送文件列表给客户端
     * @param socket 客户端socket
     */
    void sendFileList(QTcpSocket *socket);

    /**
     * @brief 准备文件头信息
     * @param fileName 文件名
     * @param fileSize 文件大小
     * @return 文件头信息数据
     */
    QByteArray prepareFileHeader(const QString &fileName, qint64 fileSize);

    /**
     * @brief 解析客户端命令
     * @param data 命令数据
     * @param socket 客户端socket
     */
    void parseCommand(const QByteArray &data, QTcpSocket *socket);

    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
    QString m_shareDir;

    // 文件传输相关
    struct FileTransfer {
        QFile *file;
        qint64 fileSize;
        qint64 bytesSent;
        QString fileName;
        bool headerSent;
    };
    QMap<QTcpSocket*, FileTransfer*> m_transfers;

    // 命令和消息常量
    static const QByteArray CMD_LIST;  // 请求文件列表命令
    static const QByteArray CMD_GET;   // 请求下载文件命令
    static const int HEADER_SIZE = 1024;
}; 