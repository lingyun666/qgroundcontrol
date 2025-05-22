#pragma once

#include <QObject>
#include <QQmlListProperty>
#include "TcpFileClient.h"

/**
 * @brief 文件信息类，用于在QML中展示文件信息
 */
class FileInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString size READ size CONSTANT)
    Q_PROPERTY(QString sizeStr READ sizeStr CONSTANT)
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)

public:
    explicit FileInfo(const QString &name, qint64 size, QObject *parent = nullptr);

    QString name() const { return m_name; }
    QString size() const { return QString::number(m_size); }
    QString sizeStr() const;
    bool selected() const { return m_selected; }
    void setSelected(bool selected);
    QString status() const { return m_status; }
    void setStatus(const QString &status);
    int progress() const { return m_progress; }
    void setProgress(int progress);

signals:
    void selectedChanged(bool selected);
    void statusChanged(const QString &status);
    void progressChanged(int progress);

private:
    QString m_name;
    qint64 m_size;
    bool m_selected;
    QString m_status;
    int m_progress;
};

/**
 * @brief TCP文件传输控制器，用于管理TCP客户端和文件列表
 */
class TcpFileTransferController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString serverAddress READ serverAddress WRITE setServerAddress NOTIFY serverAddressChanged)
    Q_PROPERTY(int serverPort READ serverPort WRITE setServerPort NOTIFY serverPortChanged)
    Q_PROPERTY(QStringList downloadFolders READ downloadFolders CONSTANT)
    Q_PROPERTY(QString selectedFolder READ selectedFolder WRITE setSelectedFolder NOTIFY selectedFolderChanged)
    Q_PROPERTY(QList<QObject*> files READ files NOTIFY filesChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString currentDownloadFile READ currentDownloadFile NOTIFY currentDownloadFileChanged)
    Q_PROPERTY(int overallProgress READ overallProgress NOTIFY overallProgressChanged)

public:
    explicit TcpFileTransferController(QObject *parent = nullptr);
    ~TcpFileTransferController();

    bool connected() const;
    QString serverAddress() const { return m_serverAddress; }
    void setServerAddress(const QString &address);
    int serverPort() const { return m_serverPort; }
    void setServerPort(int port);
    QStringList downloadFolders() const;
    QString selectedFolder() const { return m_selectedFolder; }
    void setSelectedFolder(const QString &folder);
    QList<QObject*> files() const { return m_files; }
    bool downloading() const { return m_downloading; }
    QString errorMessage() const { return m_errorMessage; }
    QString statusMessage() const { return m_statusMessage; }
    QString currentDownloadFile() const { return m_currentDownloadFile; }
    int overallProgress() const { return m_overallProgress; }

    Q_INVOKABLE void connect();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void download();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE bool openDownloadFolder();

signals:
    void connectedChanged(bool connected);
    void serverAddressChanged(const QString &address);
    void serverPortChanged(int port);
    void selectedFolderChanged(const QString &folder);
    void filesChanged();
    void downloadingChanged(bool downloading);
    void errorMessageChanged(const QString &message);
    void statusMessageChanged(const QString &message);
    void currentDownloadFileChanged(const QString &fileName);
    void overallProgressChanged(int progress);

private slots:
    void onConnectionStateChanged(bool connected);
    void onFileListReceived(const QList<QPair<QString, qint64>> &fileList);
    void onDownloadProgress(const QString &fileName, int progress);
    void onFileReceived(const QString &fileName, qint64 size);
    void onError(const QString &msg);

private:
    void clearFiles();
    void setErrorMessage(const QString &message);
    void setStatusMessage(const QString &message);
    void updateOverallProgress();
    FileInfo* findFileByName(const QString &fileName);

    TcpFileClient *m_client;
    QString m_serverAddress;
    int m_serverPort;
    QString m_selectedFolder;
    QList<QObject*> m_files;
    bool m_downloading;
    QString m_errorMessage;
    QString m_statusMessage;
    QString m_currentDownloadFile;
    int m_overallProgress;
    int m_totalFilesToDownload;
    int m_filesDownloaded;

    static const QString DEFAULT_SERVER_ADDRESS;
    static const int DEFAULT_SERVER_PORT = 8000;
}; 