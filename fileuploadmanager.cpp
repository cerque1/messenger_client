#include "fileuploadmanager.h"
#include "request.h"
#include "generaldata.h"

#include <QDir>
#include <QTimer>
#include <QDebug>
#include <QObject>

FileUploadManager::FileUploadManager(QString url, QObject* parent)
    : QObject(parent), websocket_(nullptr), url_(url) {}

void FileUploadManager::initSocket(){
    websocket_ = new QWebSocket();
    QEventLoop loop;
    connect(websocket_, SIGNAL(connected()), &loop, SLOT(quit()));
    connect(websocket_, SIGNAL(binaryMessageReceived(QByteArray)), this, SLOT(onSocketBinaryMessageReceived(QByteArray)));
    websocket_->open(QUrl(url_));
    loop.exec();
}

void FileUploadManager::uploadFiles(int chatId, int messageId,
                                    const QList<QString>& filePaths) {
    QMutexLocker locker(&mutex_);
    if(websocket_ == nullptr) {
        initSocket();
    }

    for (int i = 0; i < filePaths.size(); ++i) {
        UploadFileInfo info;
        info.originalName = QFileInfo(filePaths[i]).fileName();
        info.localPath = filePaths[i];
        info.messageId = messageId;
        info.chatId = chatId;
        info.totalBytes = QFileInfo(filePaths[i]).size();
        pendingFiles_.append(info);
    }

    currentFileIndex_ = 0;
    if (!pendingFiles_.isEmpty()) {
        currentFile_ = pendingFiles_.first();
        pendingFiles_.removeFirst();
        sendStartLoadFile(currentFile_);
    }
}

void FileUploadManager::sendStartLoadFile(const UploadFileInfo& fileInfo) {
    Request request;
    request.setPath("start_load_file");
    request.setValueToBody("chat_id", fileInfo.chatId);
    request.setValueToBody("message_id", fileInfo.messageId);
    request.setValueToBody("filename", fileInfo.originalName);
    request.setValueToBody("token", getToken());

    websocket_->sendBinaryMessage(request.getJsonRequest());
    Response resp = waitResp();
    if(resp.getStatus() != 200){
        throw std::runtime_error("error start load file");
    }
    currentFile_.contentId = resp.getValueFromBody("content_id").toInt();
    sendChunk();
}

void FileUploadManager::sendLoadFileChunk(const UploadFileInfo& fileInfo) {
    const int chunkSize = 64 * 1024;

    currentFileHandle_.setFileName(fileInfo.localPath);
    currentFileHandle_.open(QIODevice::ReadOnly);
    currentFileHandle_.seek(fileInfo.uploadedBytes);

    qint64 bytesToRead = qMin(chunkSize, fileInfo.totalBytes - fileInfo.uploadedBytes);
    QByteArray chunk = currentFileHandle_.read(bytesToRead);
    currentFileHandle_.close();

    Request request;
    request.setPath("load_file");
    request.setValueToBody("content_id", fileInfo.contentId);
    request.setValueToBody("data", QString::fromUtf8(chunk.toBase64()));
    request.setValueToBody("token", getToken());

    websocket_->sendBinaryMessage(request.getJsonRequest());
    Response resp = waitResp();
    if(resp.getStatus() != 200){
        throw std::runtime_error("error load file");
    }

    currentFile_.uploadedBytes += chunk.size();

    emit uploadProgress(fileInfo.messageId, currentFileIndex_,
                        fileInfo.uploadedBytes + chunk.size(), fileInfo.totalBytes);
    sendChunk();
}

Response FileUploadManager::waitResp(){
    std::shared_ptr<QEventLoop> loop = std::make_shared<QEventLoop>();
    std::shared_ptr<QTimer> timer = std::make_shared<QTimer>();
    timer->setSingleShot(true);

    bool timeout = false;
    std::shared_ptr<Response> resp = std::make_shared<Response>();

    QObject::connect(&*timer, &QTimer::timeout, [&]() {
        timeout = true;
        loop->quit();
    });

    QObject::connect(
        this,
        &FileUploadManager::ReceiveResp,
        [&](Response response) {
            *resp = response;
            timer->stop();
            loop->quit();
        });

    timer->start(10000);
    loop->exec();

    QObject::disconnect(this, &FileUploadManager::ReceiveResp, nullptr, nullptr);
    QObject::disconnect(data::GeneralData::GetInstance()->GetMessagesProcessor().get(),
                        &MessagesProcessor::ReceiveResponse, nullptr, nullptr);

    if (timeout) {
        throw std::runtime_error("No response from server");
    }

    return *resp;
}

void FileUploadManager::sendEndLoadFile(const UploadFileInfo& fileInfo) {
    Request request;
    request.setPath("end_load_file");
    request.setValueToBody("content_id", fileInfo.contentId);
    request.setValueToBody("token", getToken());

    websocket_->sendBinaryMessage(request.getJsonRequest());
    Response resp = waitResp();
    if(resp.getStatus() != 200){
        throw std::runtime_error("error end load file");
    }

    int newContentId = resp.getValueFromBody("new_content_id").toInt();
    currentFile_.contentId = newContentId;
    saveFileLocally(currentFile_.contentId, currentFile_.originalName,
                    currentFile_.messageId, currentFile_.chatId);

    emit uploadCompleted(currentFile_.messageId, currentFileIndex_, newContentId);

    currentFileIndex_++;
    if (!pendingFiles_.isEmpty()) {
        currentFile_ = pendingFiles_.first();
        pendingFiles_.removeFirst();
        sendStartLoadFile(currentFile_);
    }
    else {
        websocket_->deleteLater();
        websocket_ = nullptr;
    }
}

void FileUploadManager::onSocketBinaryMessageReceived(const QByteArray& data) {
    Response resp = Response::fromJson(data);
    emit ReceiveResp(resp);
}

void FileUploadManager::sendChunk() {
    if (currentFile_.contentId != -1 &&
        currentFile_.uploadedBytes < currentFile_.totalBytes) {

        sendLoadFileChunk(currentFile_);
        currentFile_.uploadedBytes += currentFileHandle_.size();
    } else if (currentFile_.uploadedBytes >= currentFile_.totalBytes) {
        sendEndLoadFile(currentFile_);
    }
}

QString FileUploadManager::getToken(){
    return data::GeneralData::GetInstance()->GetToken();
}

void FileUploadManager::saveFileLocally(int contentId, const QString& fileName,
                                        int messageId, int chatId) {
    QDir dir("temp/files");
    if (!dir.exists()) dir.mkpath(".");

    QString newFileName = QString("%1_%2_%3_%4")
                              .arg(contentId)
                              .arg(messageId)
                              .arg(chatId)
                              .arg(fileName);

    QFile::copy(currentFile_.localPath, dir.absoluteFilePath(newFileName));
}
