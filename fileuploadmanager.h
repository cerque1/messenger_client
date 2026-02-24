#ifndef FILEUPLOADMANAGER_H
#define FILEUPLOADMANAGER_H

#include "response.h"

#include <QObject>
#include <QWebSocket>
#include <QFile>
#include <QTimer>
#include <QMutex>

struct UploadFileInfo {
    QString originalName;
    QString localPath;
    int contentId = -1;
    int messageId;
    int chatId;
    qint64 uploadedBytes = 0;
    qint64 totalBytes = 0;
    bool isCompleted = false;
};

class FileUploadManager : public QObject {
    Q_OBJECT

public:
    explicit FileUploadManager(QString url, QObject* parent = nullptr);
    void uploadFiles(int chatId, int messageId, const QList<QString>& filePaths);
    void initSocket();

signals:
    void uploadProgress(int messageId, int fileIndex, qint64 uploaded, qint64 total);
    void uploadCompleted(int messageId, int fileIndex, int newContentId);
    void ReceiveResp(Response);

private slots:
    void onSocketBinaryMessageReceived(const QByteArray& data);

private:
    Response waitResp();
    void sendChunk();

    void sendStartLoadFile(const UploadFileInfo& fileInfo);
    void sendLoadFileChunk(const UploadFileInfo& fileInfo);
    void sendEndLoadFile(const UploadFileInfo& fileInfo);
    void saveFileLocally(int contentId, const QString& fileName,
                         int messageId, int chatId);
    QString getToken();

    QWebSocket* websocket_ = nullptr;
    QString url_;
    QList<UploadFileInfo> pendingFiles_;
    UploadFileInfo currentFile_;
    QFile currentFileHandle_;
    int currentFileIndex_ = 0;

    QMutex mutex_;
};

#endif
