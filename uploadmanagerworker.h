// uploadmanagerworker.h
#ifndef UPLOADMANAGERWORKER_H
#define UPLOADMANAGERWORKER_H

#include "fileuploadmanager.h"
#include <QThread>

class UploadManagerWorker : public QObject
{
    Q_OBJECT

public:
    explicit UploadManagerWorker(const QString& url)
        : url_(url), thread_(new QThread)
    {
        manager_ = new FileUploadManager(url_);
        manager_->moveToThread(thread_);

        connect(this, &UploadManagerWorker::uploadFiles,
                manager_, &FileUploadManager::uploadFiles);

        thread_->start();
    }

    FileUploadManager* GetFileManager(){
        return manager_;
    }

    ~UploadManagerWorker() {
        thread_->quit();
        thread_->wait();
        delete manager_;
        delete thread_;
    }

signals:
    void uploadFiles(int chatId, int messageId, const QList<QString>& filePaths);

public slots:
    void enqueueUpload(int chatId, int messageId, const QList<QString>& filePaths) {
        emit uploadFiles(chatId, messageId, filePaths);
    }

private:
    QString url_;
    FileUploadManager* manager_;
    QThread* thread_;
};

#endif
