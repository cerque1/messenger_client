#include "client.h"

#include <QEventLoop>
#include <QTimer>
#include <QOverload>
#include <QAbstractSocket>
#include <QThread>

Client::Client(QString server_url, std::shared_ptr<MessagesProcessor> messages_processor, QObject* parent)
    : QObject(parent), socket_(nullptr), server_url_(server_url), messages_processor_(messages_processor){
    connect(this, SIGNAL(InitWebSocket()), this, SLOT(InitWebSocketSlot()));
    connect(this, SIGNAL(NewBinaryMessage(QByteArray)), messages_processor_.get(), SLOT(OnNewMessage(QByteArray)));
}

void Client::SendRequestSlot(QByteArray message) {
    socket_->sendBinaryMessage(message);
}

void Client::OnBinaryMessageReceived(QByteArray message){
    emit NewBinaryMessage(message);
}

void Client::InitWebSocketSlot(){
    socket_ = new QWebSocket();
    QEventLoop loop;
    connect(socket_, SIGNAL(connected()), &loop, SLOT(quit()));
    connect(socket_, SIGNAL(binaryMessageReceived(QByteArray)), this, SLOT(OnBinaryMessageReceived(QByteArray)));
    connect(this, SIGNAL(SendRequest(QByteArray)), this, SLOT(SendRequestSlot(QByteArray)));
    socket_->open(QUrl(server_url_));
    loop.exec();
}

// MessagesProcessor

MessagesProcessor::MessagesProcessor(QObject* parent)
    : QObject(parent){}

void MessagesProcessor::OnNewMessage(QByteArray message){
    message_queue_.enqueue(message);
    if (!processing_message_) {
        processing_message_ = true;
        ProcessNextMessage();
    }
}

void MessagesProcessor::ProcessNextMessage(){
    if (message_queue_.isEmpty()) {
        processing_message_ = false;
        return;
    }

    QByteArray message = message_queue_.dequeue();
    ProcessMessage(message);
}

void MessagesProcessor::ProcessMessage(QByteArray data){
    try {
        if (data.contains("\"type\": \"response\"")) {
            last_response_ = Response::fromJson(data);
            emit ReceiveResponse(last_response_);
        } else {
            emit ReceiveMessage(Request::fromJson(data));
        }
    } catch(const std::exception& e) {
        qWarning() << "JSON parse failed:" << e.what() << "data:" << data.left(200);
    } catch(...) {
        qWarning() << "JSON parse failed (unknown), data:" << data.left(200);
    }
    ProcessNextMessage();
}

