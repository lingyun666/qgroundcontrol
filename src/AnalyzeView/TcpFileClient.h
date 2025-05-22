#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QDir>

/**
 * @brief TCP文件传输客户端类
 * 
 * 此类实现一个简单的TCP文件传输客户端，可以连接到服务器、获取文件列表和下载文件
 */
class TcpFileClient : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit TcpFileClient(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~TcpFileClient();
    
    /**
     * @brief 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @return 是否成功发起连接
     */
    bool connectToServer(const QString &host, quint16 port);
    
    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromServer();
    
    /**
     * @brief 请求服务器文件列表
     */
    void requestFileList();
    
    /**
     * @brief 下载文件
     * @param fileName 要下载的文件名
     * @param savePath 保存路径
     * @return 是否成功开始下载
     */
    bool downloadFile(const QString &fileName, const QString &savePath);
    
    /**
     * @brief 检查是否已连接到服务器
     * @return 是否已连接
     */
    bool isConnected() const;
    
    // 命令常量
    static const QByteArray CMD_LIST;  // "LIST"命令用于请求文件列表
    static const QByteArray CMD_GET;   // "GET"命令用于请求下载文件（不带空格）
    
signals:
    /**
     * @brief 连接状态变化信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);
    
    /**
     * @brief 收到文件列表信号
     * @param fileList 文件列表，每项为文件名和文件大小的对
     */
    void fileListReceived(const QList<QPair<QString, qint64>> &fileList);
    
    /**
     * @brief 下载进度更新信号
     * @param fileName 文件名
     * @param progress 进度百分比 (0-100)
     */
    void downloadProgress(const QString &fileName, int progress);
    
    /**
     * @brief 文件接收完成信号
     * @param fileName 文件名
     * @param size 文件大小
     */
    void fileReceived(const QString &fileName, qint64 size);
    
    /**
     * @brief 错误信号
     * @param errorMessage 错误信息
     */
    void error(const QString &errorMessage);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onError(QAbstractSocket::SocketError socketError);
    
private:
    /**
     * @brief 处理文件头信息
     * @param headerData 头信息数据
     */
    void processFileHeader(const QByteArray &headerData);
    
    /**
     * @brief 处理文件列表数据
     * @param listData 文件列表数据
     */
    void processFileList(const QByteArray &listData);
    
    /**
     * @brief 完成文件下载处理
     */
    void finishFileDownload();
    
    QTcpSocket *m_socket;     ///< TCP套接字
    QFile *m_file;            ///< 文件对象
    QString m_currentFileName; ///< 当前下载的文件名
    qint64 m_fileSize;        ///< 文件大小
    qint64 m_bytesReceived;   ///< 已接收的字节数
    
    QByteArray m_headerBuffer; ///< 头部缓冲区
    QByteArray m_fileListBuffer; ///< 文件列表缓冲区
    
    bool m_receivingHeader;    ///< 是否正在接收头部
    bool m_receivingFileList;  ///< 是否正在接收文件列表
}; 