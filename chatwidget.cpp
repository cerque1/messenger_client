#include "chatwidget.h"
#include "generaldata.h"
#include "request.h"
#include "response.h"
#include "req_resp_utils.h"
#include "utils.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextEdit>
#include <QPushButton>
#include <QMenu>
#include <QMouseEvent>
#include <QFileDialog>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QTimeZone>
#include <QFrame>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QCamera>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QImageCapture>
#include <QAudioFormat>
#include <QAudioSource>
#include <QAudioSink>
#include <QBuffer>
#include <QImage>
#include <QTimer>
#include <QPixmap>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
QAudioFormat BuildVoiceAudioFormat(const QAudioDevice& device) {
    QAudioFormat requestedFormat;
    requestedFormat.setSampleRate(16000);
    requestedFormat.setChannelCount(1);
    requestedFormat.setSampleFormat(QAudioFormat::Int16);

    if (device.isNull()) {
        return requestedFormat;
    }

    if (device.isFormatSupported(requestedFormat)) {
        return requestedFormat;
    }

    QAudioFormat fallbackFormat = device.preferredFormat();

    if (fallbackFormat.sampleRate() <= 0) {
        fallbackFormat.setSampleRate(16000);
    }
    if (fallbackFormat.channelCount() <= 0) {
        fallbackFormat.setChannelCount(1);
    }

    return fallbackFormat;
}

constexpr int kTransportSampleRate = 16000;

void AppendSenderId(QByteArray& packet, quint32 senderId) {
    packet.append(static_cast<char>(senderId & 0xFF));
    packet.append(static_cast<char>((senderId >> 8) & 0xFF));
    packet.append(static_cast<char>((senderId >> 16) & 0xFF));
    packet.append(static_cast<char>((senderId >> 24) & 0xFF));
}

bool ExtractSenderId(QByteArray& payload, quint32* senderId) {
    if (payload.size() < 4 || !senderId) {
        return false;
    }

    const unsigned char b0 = static_cast<unsigned char>(payload.at(0));
    const unsigned char b1 = static_cast<unsigned char>(payload.at(1));
    const unsigned char b2 = static_cast<unsigned char>(payload.at(2));
    const unsigned char b3 = static_cast<unsigned char>(payload.at(3));

    *senderId = static_cast<quint32>(b0)
              | (static_cast<quint32>(b1) << 8)
              | (static_cast<quint32>(b2) << 16)
              | (static_cast<quint32>(b3) << 24);

    payload = payload.mid(4);
    return true;
}

int BytesPerSample(QAudioFormat::SampleFormat sampleFormat) {
    switch (sampleFormat) {
    case QAudioFormat::UInt8:
        return 1;
    case QAudioFormat::Int16:
        return 2;
    case QAudioFormat::Int32:
    case QAudioFormat::Float:
        return 4;
    default:
        return 0;
    }
}

QByteArray ConvertAudioToTransport(const QByteArray& rawPcm, const QAudioFormat& inputFormat) {
    const int channels = std::max(1, inputFormat.channelCount());
    const int sampleRate = std::max(1, inputFormat.sampleRate());
    const int bytesPerSample = BytesPerSample(inputFormat.sampleFormat());
    if (bytesPerSample <= 0 || rawPcm.isEmpty()) {
        return {};
    }

    const int frameBytes = bytesPerSample * channels;
    const int frameCount = rawPcm.size() / frameBytes;
    if (frameCount <= 0) {
        return {};
    }

    std::vector<float> mono(static_cast<size_t>(frameCount));
    const char* data = rawPcm.constData();

    for (int frame = 0; frame < frameCount; ++frame) {
        float accum = 0.0f;
        for (int channel = 0; channel < channels; ++channel) {
            const int offset = frame * frameBytes + channel * bytesPerSample;
            float sample = 0.0f;
            if (inputFormat.sampleFormat() == QAudioFormat::Int16) {
                qint16 value = 0;
                std::memcpy(&value, data + offset, sizeof(qint16));
                sample = static_cast<float>(value) / 32768.0f;
            } else if (inputFormat.sampleFormat() == QAudioFormat::Int32) {
                qint32 value = 0;
                std::memcpy(&value, data + offset, sizeof(qint32));
                sample = static_cast<float>(value) / 2147483648.0f;
            } else if (inputFormat.sampleFormat() == QAudioFormat::Float) {
                float value = 0.0f;
                std::memcpy(&value, data + offset, sizeof(float));
                sample = value;
            } else if (inputFormat.sampleFormat() == QAudioFormat::UInt8) {
                const quint8 value = static_cast<quint8>(data[offset]);
                sample = (static_cast<float>(value) - 128.0f) / 128.0f;
            }
            accum += sample;
        }
        mono[static_cast<size_t>(frame)] = accum / static_cast<float>(channels);
    }

    const int outFrames = std::max(1, static_cast<int>(
        std::llround(static_cast<double>(frameCount) * kTransportSampleRate / sampleRate)));
    QByteArray out(static_cast<int>(outFrames * sizeof(qint16)), Qt::Uninitialized);
    qint16* outData = reinterpret_cast<qint16*>(out.data());

    for (int i = 0; i < outFrames; ++i) {
        const double srcPos = static_cast<double>(i) * sampleRate / kTransportSampleRate;
        const int srcIndex = std::clamp(static_cast<int>(srcPos), 0, frameCount - 1);
        const float clamped = std::clamp(mono[static_cast<size_t>(srcIndex)], -1.0f, 1.0f);
        outData[i] = static_cast<qint16>(clamped * 32767.0f);
    }

    return out;
}


QAudioDevice ResolveAudioInputDevice(const QByteArray& preferredId) {
    const auto devices = QMediaDevices::audioInputs();
    if (!preferredId.isEmpty()) {
        for (const QAudioDevice& device : devices) {
            if (device.id() == preferredId) {
                return device;
            }
        }
    }
    return QMediaDevices::defaultAudioInput();
}

QAudioDevice ResolveAudioOutputDevice(const QByteArray& preferredId) {
    const auto devices = QMediaDevices::audioOutputs();
    if (!preferredId.isEmpty()) {
        for (const QAudioDevice& device : devices) {
            if (device.id() == preferredId) {
                return device;
            }
        }
    }
    return QMediaDevices::defaultAudioOutput();
}

QCameraDevice ResolveCameraDevice(const QByteArray& preferredId) {
    const auto devices = QMediaDevices::videoInputs();
    if (!preferredId.isEmpty()) {
        for (const QCameraDevice& device : devices) {
            if (device.id() == preferredId) {
                return device;
            }
        }
    }

    if (!devices.isEmpty()) {
        return devices.first();
    }

    return QCameraDevice();
}

QByteArray ConvertTransportToOutput(const QByteArray& transportPcm, const QAudioFormat& outputFormat) {
    if (transportPcm.isEmpty()) {
        return {};
    }

    const int outChannels = std::max(1, outputFormat.channelCount());
    const int outRate = std::max(1, outputFormat.sampleRate());
    const int outBps = BytesPerSample(outputFormat.sampleFormat());
    if (outBps <= 0) {
        return {};
    }

    const int inFrames = transportPcm.size() / static_cast<int>(sizeof(qint16));
    if (inFrames <= 0) {
        return {};
    }

    const qint16* inData = reinterpret_cast<const qint16*>(transportPcm.constData());
    const int outFrames = std::max(1, static_cast<int>(
        std::llround(static_cast<double>(inFrames) * outRate / kTransportSampleRate)));
    QByteArray out(outFrames * outChannels * outBps, Qt::Uninitialized);
    char* outData = out.data();

    for (int i = 0; i < outFrames; ++i) {
        const double srcPos = static_cast<double>(i) * kTransportSampleRate / outRate;
        const int srcIndex = std::clamp(static_cast<int>(srcPos), 0, inFrames - 1);
        const float sample = static_cast<float>(inData[srcIndex]) / 32768.0f;

        for (int ch = 0; ch < outChannels; ++ch) {
            const int offset = (i * outChannels + ch) * outBps;
            if (outputFormat.sampleFormat() == QAudioFormat::Int16) {
                const qint16 v = static_cast<qint16>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
                std::memcpy(outData + offset, &v, sizeof(qint16));
            } else if (outputFormat.sampleFormat() == QAudioFormat::Int32) {
                const qint32 v = static_cast<qint32>(std::clamp(sample, -1.0f, 1.0f) * 2147483647.0f);
                std::memcpy(outData + offset, &v, sizeof(qint32));
            } else if (outputFormat.sampleFormat() == QAudioFormat::Float) {
                const float v = std::clamp(sample, -1.0f, 1.0f);
                std::memcpy(outData + offset, &v, sizeof(float));
            } else if (outputFormat.sampleFormat() == QAudioFormat::UInt8) {
                const quint8 v = static_cast<quint8>(std::clamp(sample * 127.0f + 128.0f, 0.0f, 255.0f));
                std::memcpy(outData + offset, &v, sizeof(quint8));
            }
        }
    }

    return out;
}
}

MessageWidget::MessageWidget(const entities::Message& message, bool isNew, QWidget* parent)
    : QWidget(parent), chat_id_(message.chat_id_), message_id_(message.id_),
    current_status_(message.status_), sender_id_(message.sender_id_), is_changed_(message.is_changed_) {

    QHBoxLayout* main_layout = new QHBoxLayout(this);
    main_layout->setObjectName("message_layout");

    message_block_layout = new QVBoxLayout();

    files_list_ = new FilesListWidget();
    files_list_->setFiles(message.files_, isNew, message.id_, message.chat_id_);
    connect(files_list_, &FilesListWidget::fileDownloadRequested,
            this, &MessageWidget::onFileDownloadRequested);
    if(isNew){
        connect(this, &MessageWidget::SetProgress, files_list_, &FilesListWidget::SetLoadProgress);
        connect(this, &MessageWidget::SetComplete, files_list_, &FilesListWidget::SetCompleteLoad);
    }

    QLabel* label = new QLabel(message.text_);
    label->setObjectName("message_label");
    label->setWordWrap(true);
    label->setStyleSheet("background-color: transparent; padding: 5px 2px 5px 5px; max-width: 300px; color: #ffffff;");

    is_changed_label_ = new QLabel("Изменено");
    is_changed_label_->setStyleSheet("background-color: transparent; padding-left: 5px; font-size: 8px; color: #a1a1aa;");
    is_changed_label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QDateTime time = QDateTime::fromString(
        message.create_time_,
        Qt::ISODate
    );
    time.setTimeZone(QTimeZone::UTC);
    time = time.toLocalTime();
    QString time_text = time.isValid() ? time.toString("dd.MM.yy HH:mm") : message.create_time_;
    QLabel* send_time_label = new QLabel(time_text);
    send_time_label->setObjectName("time_label");
    send_time_label->setStyleSheet("background-color: transparent; padding-left: 5px; font-size: 10px; color: #a1a1aa;");
    send_time_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    message_block_layout->addWidget(files_list_);
    if (message.files_.size() == 0){
        files_list_->hide();
    }
    message_block_layout->addWidget(label);
    message_block_layout->addWidget(is_changed_label_);
    if (!message.is_changed_) {
        is_changed_label_->hide();
    }
    message_block_layout->addWidget(send_time_label);

    message_block_layout->setAlignment(is_changed_label_, Qt::AlignLeft);
    message_block_layout->setAlignment(send_time_label, Qt::AlignRight);
    message_block_layout->setContentsMargins(5, 5, 5, 5);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

void MessageWidget::onFileDownloadRequested(int messageId, int fileIndex, QString filename){
    emit fileDownloadRequested(chat_id_, messageId, fileIndex, filename);
}

void MessageWidget::setMessage(QString message){
    QLabel* label = findChild<QLabel*>("message_label");
    label->setText(message);
}

void MessageWidget::setTime(QString time){
    QLabel* label = findChild<QLabel*>("time_label");
    label->setText(time);
}

void MessageWidget::deleteActionSlot(){
    emit deleteAction(false);
}

void MessageWidget::uploadProgressSlot(int messageId, int fileIndex, qint64 uploaded, qint64 total) {
    emit SetProgress(messageId, fileIndex, uploaded, total);
}

void MessageWidget::uploadCompletedSlot(int messageId, int fileIndex, int newContentId) {
    emit SetComplete(messageId, fileIndex, newContentId);
}

OwnMessage::OwnMessage(const entities::Message& message, bool isNew, QWidget* parent)
    : MessageWidget(message, isNew, parent){
    QHBoxLayout* main_layout = findChild<QHBoxLayout*>("message_layout");

    QWidget* message_block = new QWidget();
    message_block->setObjectName("message_block");
    message_block->setMaximumWidth(400);

    QLabel* status_label = new QLabel(QString::number(message.status_));
    status_label->setObjectName("status_label");
    status_label->setWordWrap(true);

    main_layout->addStretch();
    message_block->setLayout(message_block_layout);
    message_block->setStyleSheet(
        "QWidget#message_block {"
        "    background-color: #1e3a8a;"
        "    border-radius: 10px;"
        "    padding: 8px 12px;"
        "}");
    main_layout->addWidget(message_block);
    main_layout->addWidget(status_label);
    status_label->setStyleSheet("color: #a1a1aa; font-size: 10px; padding-left: 5px;");

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenuSlot(QPoint)));
}

void OwnMessage::setStatus(int status){
    QLabel* label = findChild<QLabel*>("status_label");
    label->setText(QString::number(status));
}

void OwnMessage::showContextMenuSlot(const QPoint &pos){
    QPoint point_pos;
    if (sender()->inherits("QAbstractScrollArea"))
        point_pos = dynamic_cast<QAbstractScrollArea*>(sender())->viewport()->mapToGlobal(pos);
    else
        point_pos = dynamic_cast<QWidget*>(sender())->mapToGlobal(pos);

    QMenu* message_menu = new QMenu(this);
    message_menu->setStyleSheet(
        "QMenu {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    color: #ffffff;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    padding: 8px 16px;"
        "    border-radius: 6px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #2563eb;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #23262b;"
        "    margin: 4px 8px;"
        "}");
    QAction* update_action = new QAction(QString::fromUtf8("Изменить"), this);
    connect(update_action, SIGNAL(triggered(bool)), this, SLOT(updateActionSlot()));
    QAction* full_delete_action = new QAction(QString::fromUtf8("Удалить для всех"), this);
    connect(full_delete_action, SIGNAL(triggered(bool)), this, SLOT(fullDeleteActionSlot()));
    QAction* delete_action = new QAction(QString::fromUtf8("Удалить для себя"), this);
    connect(delete_action, SIGNAL(triggered(bool)), this, SLOT(deleteActionSlot()));
    message_menu->addAction(update_action);
    message_menu->addSeparator();
    message_menu->addAction(full_delete_action);
    message_menu->addSeparator();
    message_menu->addAction(delete_action);
    message_menu->popup(point_pos);
}

void OwnMessage::fullDeleteActionSlot(){
    emit deleteAction(true);
}

void OwnMessage::updateActionSlot(){
    emit changeMessageSignal();
}

InterlocutorMessage::InterlocutorMessage(const entities::Message& message, bool is_dialog, QWidget* parent)
    : MessageWidget(message, false, parent)
{
    QHBoxLayout* main_layout = findChild<QHBoxLayout*>("message_layout");

    QWidget* container = new QWidget;
    QVBoxLayout* v = new QVBoxLayout(container);
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(2);

    if(!is_dialog){
        QLabel* username = new QLabel("НН");
        username->setStyleSheet("color: #60a5fa; font-size: 11px; font-weight: 500;");
        username->setAlignment(Qt::AlignLeft);

        auto users = data::GeneralData::GetInstance()->getMembersToChat(message.chat_id_)->users_;
        if(users.find(message.sender_id_) != users.end()){
            username->setText(users[message.sender_id_].name_);
        }

        v->addWidget(username);
    }

    QWidget* message_block = new QWidget;
    message_block->setObjectName("message_block");
    message_block->setMaximumWidth(400);

    message_block->setLayout(message_block_layout);

    message_block->setStyleSheet(
        "QWidget#message_block {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    padding: 8px 12px;"
        "}");

    v->addWidget(message_block);

    main_layout->addWidget(container);
    main_layout->addStretch();

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showContextMenuSlot(QPoint)));
}


void InterlocutorMessage::setStatus(int /*status*/){
    // Убрали стили для message_block, так как теперь он прозрачный
}

void InterlocutorMessage::showContextMenuSlot(const QPoint &pos){
    QPoint point_pos;
    if (sender()->inherits("QAbstractScrollArea"))
        point_pos = dynamic_cast<QAbstractScrollArea*>(sender())->viewport()->mapToGlobal(pos);
    else
        point_pos = dynamic_cast<QWidget*>(sender())->mapToGlobal(pos);

    QMenu* message_menu = new QMenu(this);
    message_menu->setStyleSheet(
        "QMenu {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    color: #ffffff;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    padding: 8px 16px;"
        "    border-radius: 6px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #2563eb;"
        "}");
    QAction* delete_action = new QAction(QString::fromUtf8("Удалить для себя"), this);
    connect(delete_action, SIGNAL(triggered(bool)), this, SLOT(deleteActionSlot()));
    message_menu->addAction(delete_action);
    message_menu->popup(point_pos);
}

MessagesWidget::MessagesWidget(int chat_id, bool is_dialog, QWidget *parent)
    : QScrollArea(parent), chat_id_(chat_id), is_dialog_(is_dialog){
    this->setObjectName("messages_scroll_area" + QString::number(chat_id_));
    this->setWidgetResizable(true);
    this->setStyleSheet(
        "QScrollArea#messages_scroll_area" + QString::number(chat_id_) + " {"
        "    background-color: #0b0c0e;"
        "    border: none;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: #15171a;"
        "    width: 8px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #23262b;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #2f3339;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}");

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(ValueChanged(int)));

    QWidget* messages_container = new QWidget(this);
    messages_container->setStyleSheet("background-color: #0b0c0e;");
    QVBoxLayout* messages_layout_ = new QVBoxLayout(messages_container);
    messages_layout_->setAlignment(Qt::AlignBottom);
    messages_layout_->setContentsMargins(8, 8, 8, 8);
    messages_layout_->setSpacing(8);
    messages_layout_->setObjectName("messages_layout" + QString::number(chat_id_));
    this->setWidget(messages_container);
}

QList<MessageWidget*> MessagesWidget::addMessages(const QList<entities::Message>& messages){
    auto messages_layout = findChild<QVBoxLayout*>("messages_layout" + QString::number(chat_id_));
    QList<MessageWidget*> widgets_;
    MessageWidget* last_widget;

    int accumulated_scroll_value = 0;
    int top_margin = messages_layout->contentsMargins().top();
    int bottom_margin = messages_layout->contentsMargins().bottom();

    accumulated_scroll_value += top_margin;

    for(const entities::Message& message : messages){
        last_widget = CreateMessageWidget(message, false);
        widgets_.push_back(last_widget);
        messages_layout->addWidget(last_widget);
        last_id_ = message.id_;
        messages_[message.id_] = last_widget;

        if (accumulated_scroll_value > top_margin) {
            accumulated_scroll_value += messages_layout->spacing();
        }
        accumulated_scroll_value += last_widget->height();

        int user_id = data::GeneralData::GetInstance()->GetUserId();
        if(message.sender_id_ != user_id && message.status_ == 1){
            unread_messages_.push_back(message.id_);
            if(!first_unread_message_.has_value()){
                first_unread_message_ = message.id_;
            }
        }
    }

    accumulated_scroll_value += bottom_margin;

    QTimer::singleShot(50, [this]() {
        this->widget()->updateGeometry();
        if (auto layout = this->widget()->layout()) {
            layout->activate();
        }

        int value = SetUnreadMessageLine();
        qDebug() << verticalScrollBar()->maximum() << viewport()->height();
        if(value > 0 && verticalScrollBar()->maximum() > viewport()->height()){
            this->verticalScrollBar()->setValue(value - viewport()->height());
        } else if(verticalScrollBar()->maximum() <= viewport()->height()) {
            ValueChanged(verticalScrollBar()->maximum());
        } else {
            this->verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        }
    });
    return widgets_;
}

MessageWidget* MessagesWidget::addMessage(const entities::Message& message){
    if(message.id_ <= last_id_){
        return nullptr;
    }
    int pre_scroll_value = this->verticalScrollBar()->maximum();
    MessageWidget* widget = CreateMessageWidget(message, false);
    findChild<QVBoxLayout*>("messages_layout" + QString::number(chat_id_))->addWidget(widget);
    if(message.sender_id_ != data::GeneralData::GetInstance()->GetUserId() && message.status_ == 1){
        unread_messages_.push_back(message.id_);
    }

    this->widget()->updateGeometry();
    QApplication::processEvents();
    last_id_ = message.id_;
    messages_[message.id_] = widget;

    if(message.sender_id_ == data::GeneralData::GetInstance()->GetUserId()){
        QTimer::singleShot(0, [this]() {
            this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
        });
    }
    else{
        if(pre_scroll_value == this->verticalScrollBar()->value()){
            QTimer::singleShot(0, [this]() {
                this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
            });
        }
    }

    return widget;
}

MessageWidget* MessagesWidget::addPreMessage(const entities::Message& message){
    if(message.id_ <= last_id_){
        return nullptr;
    }
    int pre_scroll_value = this->verticalScrollBar()->maximum();
    MessageWidget* widget = CreateMessageWidget(message, true);
    findChild<QVBoxLayout*>("messages_layout" + QString::number(chat_id_))->addWidget(widget);
    this->widget()->updateGeometry();
    QApplication::processEvents();
    pre_messages_[message.id_] = widget;

    if(message.sender_id_ == data::GeneralData::GetInstance()->GetUserId()){
        QTimer::singleShot(0, [this]() {
            this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
        });
    }
    else{
        if(pre_scroll_value == this->verticalScrollBar()->value()){
            QTimer::singleShot(0, [this]() {
                this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
            });
        }
    }

    return widget;
}

void MessagesWidget::deleteMessage(int message_id){
    if(messages_.find(message_id) == messages_.end()){
        return;
    }
    findChild<QVBoxLayout*>("messages_layout" + QString::number(chat_id_))->removeWidget(messages_[message_id]);

    if(messages_[message_id]->getCurrentStatus() == 1 && messages_[message_id]->getSenderId() != data::GeneralData::GetInstance()->GetUserId()){
        // if(!scroll_value_to_unread_message_id_.isEmpty() && *first_unread_message_ != scroll_value_to_unread_message_id_.first()){
        //     first_unread_message_ = scroll_value_to_unread_message_id_.first();
        // }
        unread_messages_.removeAt(unread_messages_.indexOf(message_id));
        if(*first_unread_message_ == message_id){
            first_unread_message_ = unread_messages_.first();
        }

        SetUnreadMessageLine();
    }
    messages_[message_id]->deleteLater();
    messages_.remove(message_id);

    if(last_id_ == message_id){
        last_id_ = messages_.lastKey();
    }
}

void MessagesWidget::changeMessageInfo(int pre_id, int id, QString time, int status){
    MessageWidget* widget = pre_messages_[pre_id];
    widget->setStatus(status);
    widget->setTime(time);
    pre_messages_.remove(pre_id);
    widget->setId(id);
    messages_[id] = widget;
}

void MessagesWidget::updateMessageStatus(const entities::Status& status){
    for(auto i = messages_.find(status.message_id_); i != messages_.begin(); --i){
        if((*i)->getCurrentStatus() == 2){
            break;
        }
        (*i)->setStatus(status.status_);
    }
}

void MessagesWidget::changeMessage(int message_id, QString text){
    messages_[message_id]->changeMessage(text);
}

int MessagesWidget::SetUnreadMessageLine(){
    QVBoxLayout* layout = findChild<QVBoxLayout*>("messages_layout" + QString::number(chat_id_));
    if(unread_message_line_){
        unread_message_line_->hide();
        layout->removeWidget(unread_message_line_);
        unread_message_line_->deleteLater();
        unread_message_line_ = nullptr;
    }

    if(unread_messages_.isEmpty()){
        return -1;
    }

    int last_unread_message_index = layout->indexOf(messages_[*first_unread_message_]);
    unread_message_line_ = new QLabel(this);
    unread_message_line_->setAlignment(Qt::AlignCenter);
    unread_message_line_->setText("непрочитанные сообщения");
    unread_message_line_->setStyleSheet(
        "QLabel {"
        "    color: #60a5fa;"
        "    font-size: 12px;"
        "    font-weight: 500;"
        "    padding: 4px 0px;"
        "}");

    layout->insertWidget(last_unread_message_index, unread_message_line_);
    unread_message_line_->show();
    layout->activate();

    QRect widget_rect = unread_message_line_->geometry();
    int y_pos = widget_rect.bottom();
    qDebug() << y_pos;
    return y_pos;
}

void MessagesWidget::OnContentClick(int message_id){
    auto message_widget = messages_[message_id];
    QPoint pos = message_widget->mapTo(widget(), QPoint(0, 0));
    verticalScrollBar()->setValue(pos.y());
}

void MessagesWidget::ValueChanged(int)
{
    std::lock_guard<std::mutex> guard(mu);

    if (unread_messages_.isEmpty())
        return;

    int viewportTop = 0;
    int viewportBottom = viewport()->height();

    int last_visible_unread_message_id = -1;

    for (int unread_message_id : unread_messages_) {
        auto* message = messages_[unread_message_id];

        int messageBottom =
            message->mapTo(viewport(),
                           QPoint(0, message->height())).y();

        if (messageBottom >= viewportTop &&
            messageBottom <= viewportBottom)
        {
            last_visible_unread_message_id = unread_message_id;
        }
        else if (last_visible_unread_message_id != -1) {
            break;
        }
    }

    if (last_visible_unread_message_id == -1)
        return;

    Request req = req_resp_utils::MakeNewStatusRequest(
        chat_id_,
        last_visible_unread_message_id,
        2,
        data::GeneralData::GetInstance()->GetToken());

    auto client = data::GeneralData::GetInstance()->GetClient();
    Response resp;

    try {
        QMetaObject::invokeMethod(
            client.get(),
            [client, req, &resp]() {
                resp = req_resp_utils::SendReqAndWaitResp(req);
            },
            Qt::BlockingQueuedConnection);
    } catch (std::runtime_error& e) {
        qDebug() << e.what();
    }

    if (resp.getStatus() != 200) {
        utils::MakeMessageBox(
            resp.getValueFromBody("message").toString());
        return;
    }

    int i;
    for (i = 0; i < unread_messages_.size(); ++i) {
        int unread_message_id = unread_messages_[i];
        messages_[unread_message_id]->setStatus(2);

        if (unread_message_id == last_visible_unread_message_id)
            break;
    }

    for (int j = 0; j <= i; ++j) {
        unread_messages_.removeAt(0);
    }

    if (unread_messages_.isEmpty())
        first_unread_message_ = std::nullopt;
    else
        first_unread_message_ = unread_messages_.first();
}

void MessagesWidget::OnDeleteMessageAction(bool full_delete){
    MessageWidget* message_widget = qobject_cast<MessageWidget*>(sender());
    int message_id = message_widget->getMessageId();

    Request req = req_resp_utils::MakeDeleteMessageRequest(data::GeneralData::GetInstance()->GetToken(),
                                                           chat_id_,
                                                           message_id,
                                                           full_delete);

    Response resp = req_resp_utils::SendReqAndWaitResp(req);
    if(resp.getStatus() != 200){
        utils::MakeMessageBox(resp.getValueFromBody("message").toString());
        return;
    }

    findChild<QVBoxLayout*>("messages_layout" + QString::number(chat_id_))->removeWidget(messages_[message_id]);
    if(messages_.find(message_id) != messages_.end()){
        messages_[message_id]->deleteLater();
        messages_.remove(message_id);
    }
    else {
        pre_messages_[message_id]->deleteLater();
        pre_messages_.remove(message_id);
    }

    if(!messages_.isEmpty() && last_id_ == message_id){
        messages_.firstKey();
    }
    if(message_widget->getCurrentStatus() == 1 || message_widget->getSenderId() != data::GeneralData::GetInstance()->GetUserId()){
        if(!unread_messages_.isEmpty() && *first_unread_message_ != unread_messages_.first()){
            first_unread_message_ = unread_messages_.first();
        }

        SetUnreadMessageLine();
    }
}

MessageWidget* MessagesWidget::CreateMessageWidget(entities::Message message, bool isNew){
    if(message.sender_id_ == data::GeneralData::GetInstance()->GetUserId()){
        OwnMessage* own_message = new OwnMessage(message, isNew, this);
        connect(own_message, SIGNAL(deleteAction(bool)), this, SLOT(OnDeleteMessageAction(bool)));
        connect(own_message, SIGNAL(changeMessageSignal()), this, SLOT(OnChangeMessageSlot()));
        return own_message;
    }

    MessageWidget* interlocutor_message = new InterlocutorMessage(message, is_dialog_, this);
    connect(interlocutor_message, SIGNAL(deleteAction(bool)), this, SLOT(OnDeleteMessageAction(bool)));
    connect(interlocutor_message, SIGNAL(changeMessageSignal()), this, SLOT(OnChangeMessageSlot()));
    return interlocutor_message;
}

void MessageWidget::fileDownloadProgressSlot(int messageId, int fileIndex, qint64 bytesReceived, qint64 total) {
    emit files_list_->fileDownloadProgressSlot(messageId, fileIndex, bytesReceived, total);
}

void MessageWidget::fileDownloadCompletedSlot(int messageId, int fileIndex) {
    emit files_list_->fileDownloadCompletedSlot(messageId, fileIndex);
}

MessageInfoWidget::MessageInfoWidget(QWidget* parent)
    : QWidget(parent){
    this->setMaximumHeight(100);
    QHBoxLayout* general_layout = new QHBoxLayout(this);
    general_layout->setContentsMargins(8, 8, 8, 8);
    setStyleSheet("background-color: #15171a; border: none; padding: 8px;");
    header_ = new QLabel(this);
    header_->setStyleSheet("font-weight: 500; font-size: 12px; color: #ffffff;");
    message_content_ = new QLabel(this);
    message_content_->setStyleSheet("font-size: 10px; color: #a1a1aa;");

    QWidget* layout_widget = new QWidget(this);
    QVBoxLayout* message_info = new QVBoxLayout();
    message_info->setSpacing(2);
    layout_widget->setLayout(message_info);

    message_info->addWidget(header_);
    message_info->addWidget(message_content_);

    QPushButton* close_button = new QPushButton(this);
    close_button->setText("×");
    close_button->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    color: #a1a1aa;"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    color: #ffffff;"
        "}");
    connect(close_button, SIGNAL(clicked(bool)), this, SIGNAL(CloseMessageInfo()));
    close_button->setMaximumWidth(40);
    close_button->setMaximumHeight(40);

    general_layout->addWidget(layout_widget);
    general_layout->addWidget(close_button);
}

void MessageInfoWidget::SetMessageHeader(QString header){
    header_->setText(header);
}

void MessageInfoWidget::SetMessage(MessageWidget* message){
    message_ = message;
    message_content_->setText(message->getMessage());
    connect(message, SIGNAL(changeMessageSignal()), this, SLOT(OnMessageUpdate()));
}

void MessageInfoWidget::Clear(){
    disconnect(message_, SIGNAL(changeMessageSignal()), this, SLOT(OnMessageUpdate()));
    message_ = nullptr;
    header_->clear();
    message_content_->clear();
}

void MessageInfoWidget::OnMessageUpdate(){
    message_content_->setText(message_->getMessage());
}

InputPanelWidget::InputPanelWidget(QWidget *parent)
    : QWidget(parent){
    this->setStyleSheet("QWidget { background-color: #0b0c0e; }");
    QVBoxLayout* general_layout = new QVBoxLayout(this);
    general_layout->setContentsMargins(8, 8, 8, 8);
    general_layout->setSpacing(8);
    message_info_ = new MessageInfoWidget(this);

    files_display_ = new FilesDisplayWidget(this);
    files_display_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(files_display_, SIGNAL(removeFile(QString)),
            this, SLOT(OnFileRemoved(QString)));

    QWidget* input_widget = new QWidget(this);
    QHBoxLayout* input_layout = new QHBoxLayout();
    input_widget->setLayout(input_layout);

    QPushButton* file_button = new QPushButton("файл");
    file_button->setObjectName("file_button");
    file_button->setStyleSheet(
        "QPushButton#file_button {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    color: #ffffff;"
        "    font-weight: 500;"
        "    padding: 7px 16px;"
        "}"
        "QPushButton#file_button:hover {"
        "    background-color: #181b1f;"
        "    border: 1px solid #2f3339;"
        "}"
        "QPushButton#file_button:pressed {"
        "    background-color: #1b1e23;"
        "}");
    input_layout->addWidget(file_button);
    connect(file_button, SIGNAL(clicked(bool)), this, SLOT(ClickToFileButtonSlot()));

    message_input_ = new QTextEdit();
    message_input_->setObjectName("message_input");
    message_input_->setFrameShape(QFrame::NoFrame);
    message_input_->setMinimumHeight(30);
    QScreen* screen = QGuiApplication::primaryScreen();
    int screenHeight = screen ? screen->geometry().height() : 1080; // fallback к 1080 если экран не найден
    message_input_->setMaximumHeight(static_cast<int>(screenHeight * 0.175));
    
    // Отключаем горизонтальную прокрутку
    message_input_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    message_input_->setLineWrapMode(QTextEdit::WidgetWidth);
    
    // Вертикальная прокрутка скрыта, пока не достигнута максимальная высота
    message_input_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    message_input_->setStyleSheet(
        "QTextEdit#message_input {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    padding: 5px 12px;"
        "    color: #ffffff;"
        "    selection-background-color: #3b82f6;"
        "}"
        "QTextEdit#message_input:hover {"
        "    background-color: #181b1f;"
        "    border: 1px solid #2f3339;"
        "}"
        "QTextEdit#message_input:focus {"
        "    background-color: #1b1e23;"
        "    border: 1px solid #3b82f6;"
        "}"
        "QTextEdit#message_input QAbstractScrollArea::viewport {"
        "    background-color: transparent;"
        "    border: none;"
        "}"
        "QTextEdit#message_input QScrollBar:vertical {"
        "    background-color: transparent;"
        "    width: 8px;"
        "    border-radius: 4px;"
        "}"
        "QTextEdit#message_input QScrollBar::handle:vertical {"
        "    background-color: #23262b;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        "QTextEdit#message_input QScrollBar::handle:vertical:hover {"
        "    background-color: #2f3339;"
        "}"
        "QTextEdit#message_input QScrollBar::add-line:vertical, "
        "QTextEdit#message_input QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}");
    
    // Устанавливаем отступы документа для правильного отображения скругленных углов
    message_input_->document()->setDocumentMargin(0);
    
    // Устанавливаем атрибут для обрезки содержимого по границам виджета
    message_input_->setAttribute(Qt::WA_OpaquePaintEvent, false);
    
    input_layout->addWidget(message_input_);
    
    // Подключаем сигнал изменения текста для динамического изменения высоты
    connect(message_input_, &QTextEdit::textChanged, this, &InputPanelWidget::adjustInputHeight);

    send_button_ = new QPushButton("Отправить");
    send_button_->setObjectName("send_button");
    send_button_->setStyleSheet(
        "QPushButton#send_button {"
        "    background-color: #2563eb;"
        "    border: 1px solid #2563eb;"
        "    border-radius: 10px;"
        "    color: white;"
        "    font-weight: 600;"
        "    padding: 7px 16px;"
        "}"
        "QPushButton#send_button:hover {"
        "    background-color: #3b82f6;"
        "    border: 1px solid #3b82f6;"
        "}"
        "QPushButton#send_button:pressed {"
        "    background-color: #1d4ed8;"
        "    border: 1px solid #1d4ed8;"
        "}");
    input_layout->addWidget(send_button_);
    connect(send_button_, SIGNAL(clicked(bool)), this, SIGNAL(ClickSendMessage()));
    connect(message_info_, SIGNAL(CloseMessageInfo()), this, SLOT(ClickToCancelSlot()));

    general_layout->addWidget(message_info_);
    message_info_->hide();
    general_layout->addWidget(files_display_);
    general_layout->addWidget(input_widget);

    setLayout(general_layout);
}

void InputPanelWidget::ClickToFileButtonSlot(){
    QString filePath = QFileDialog::getOpenFileName(
        this,                        // Родительское окно
        "Выберите файл",           // Заголовок
        "",                         // Директория по умолчанию (пустая = текущая)
        "Все файлы (*.*)"           // Фильтр
        );

    if (filePath.isEmpty()) {
        qDebug() << "Файл не выбран";
        return;
    }
    qDebug() << filePath;
    emit ChoseFile(filePath);
    files_display_->addFileName(filePath);
}

void InputPanelWidget::OnFileRemoved(const QString& fileName) {
    qDebug() << "Файл удален из UI:" << fileName;
}

void InputPanelWidget::ShowMessageInfo(QString header, MessageWidget* message){
    message_info_->SetMessageHeader(header);
    message_info_->SetMessage(message);
    message_info_->show();
}

void InputPanelWidget::HideMessageInfo(){
    message_info_->hide();
}

void InputPanelWidget::ClickToCancelSlot(){
    message_info_->Clear();
    emit ClickToCancel();
}

void InputPanelWidget::ClearFiles(){
    files_display_->clearFiles();
}

void InputPanelWidget::resizeEvent(QResizeEvent* event){
    QWidget::resizeEvent(event);
    // Пересчитываем высоту при изменении размера виджета
    adjustInputHeight();
}

void InputPanelWidget::adjustInputHeight(){
    if(!message_input_) return;
    
    // Вычисляем необходимую высоту на основе содержимого
    // Используем documentLayout для более точного расчета
    QTextDocument* document = message_input_->document();
    
    // Устанавливаем ширину текста с учетом padding и border (12px слева + 12px справа + 8px для скроллбара если нужен)
    int availableWidth = message_input_->width() - 24; // padding: 12px * 2
    document->setTextWidth(availableWidth);
    
    qreal docHeight = document->documentLayout()->documentSize().height();
    
    int minHeight = 30;
    
    // Получаем высоту экрана для расчета максимума
    QScreen* screen = QGuiApplication::primaryScreen();
    int screenHeight = screen ? screen->geometry().height() : 1080; // fallback к 1080 если экран не найден
    int maxHeight = static_cast<int>(screenHeight * 0.175);
    
    // Вычисляем необходимую высоту с учетом отступов (padding: 5px сверху и снизу = 10px)
    int contentHeight = static_cast<int>(docHeight);
    int newHeight = qBound(minHeight, contentHeight + 10, maxHeight);
    
    message_input_->setFixedHeight(newHeight);
    
    // Вертикальная прокрутка только после достижения максимальной высоты
    if (newHeight >= maxHeight && contentHeight > maxHeight - 10) {
        message_input_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        message_input_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

ChatWidget::ChatWidget(int chat_id, std::shared_ptr<UploadManagerWorker> upload_manager_worker, QWidget *parent)
    : QWidget(parent)
    , chat_id_(chat_id)
    , upload_manager_worker_(upload_manager_worker)
    , download_manager_worker_(std::make_unique<DownloadManagerWorker>(QString::fromStdString("ws://localhost:1234")))
    , call_session_(std::make_unique<CallSession>())
{
    const int currentUserId = data::GeneralData::GetInstance()->GetUserId();
    if (currentUserId > 0) {
        local_media_sender_id_ = static_cast<quint32>(currentUserId);
    } else {
        local_media_sender_id_ = QRandomGenerator::global()->generate();
        if (local_media_sender_id_ == 0) {
            local_media_sender_id_ = 1;
        }
    }

    this->setStyleSheet("QWidget { background-color: #0b0c0e; color: #e6e6e6; }");
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    ChatHeader* header = new ChatHeader(-1, "", "", this);
    MessagesWidget* messages = new MessagesWidget(chat_id_, true, this);
    input_panel_ = new InputPanelWidget(this);
    connect(input_panel_, SIGNAL(ClickSendMessage()), this, SLOT(ClickToSendMessage()));
    connect(input_panel_, SIGNAL(ChoseFile(QString)), this, SLOT(ChoseFile(QString)));
    connect(call_session_.get(), &CallSession::localOfferCreated, this, &ChatWidget::SendOffer);
    connect(call_session_.get(), &CallSession::localAnswerCreated, this, &ChatWidget::SendAnswer);
    connect(call_session_.get(), &CallSession::localCandidateCreated, this, &ChatWidget::SendCandidate);
    connect(call_session_.get(), &CallSession::mediaPacketReceived, this, &ChatWidget::OnMediaPacketReceived);
    connect(call_session_.get(), &CallSession::error, [](const QString& message){ utils::MakeMessageBox(message); });
    connect(header, SIGNAL(startCallClicked(int,QString)), this, SLOT(StartCall(int,QString)));
    messages_in_chats_[chat_id] = {header, messages};

    main_layout->addWidget(header);
    main_layout->addWidget(messages);
    main_layout->addWidget(input_panel_);

    main_layout->setStretch(0, 0);
    main_layout->setStretch(1, 1);
    main_layout->setStretch(2, 0);
}

void ChatWidget::OnContentClick(int message_id){
    messages_in_chats_[chat_id_].messages->OnContentClick(message_id);
}

void ChatWidget::ClickToSendMessage(){
    QTextEdit* message_edit = input_panel_->getMessageInput();
    QString message = message_edit->toPlainText();
    if(message.isEmpty()){
        return;
    }

    std::srand(time(NULL));
    int pre_id = std::rand() % 10000;
    entities::Message message_for_widget{pre_id,
                                         chat_id_,
                                         data::GeneralData::GetInstance()->GetUserId(),
                                         message,
                                         QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                                         0,
                                         false,
                                         messages_in_chats_[chat_id_].messages->getFiles()};


    auto message_w = AddPreMessageToChat(message_for_widget);
    Request req = req_resp_utils::MakeSendMessageRequest(data::GeneralData::GetInstance()->GetToken(),
                                                         pre_id,
                                                         chat_id_,
                                                         message,
                                                         message_for_widget.files_.size());
    auto client = data::GeneralData::GetInstance()->GetClient();
    Response resp;
    try {
        QMetaObject::invokeMethod(client.get(), [client, req, &resp]() {
            resp = req_resp_utils::SendReqAndWaitResp(req);
        }, Qt::BlockingQueuedConnection);
    } catch(std::runtime_error& e) {
        qDebug() << e.what();
    }

    if(resp.getStatus() != 200){
        utils::MakeMessageBox(resp.getValueFromBody("message").toString());
        return;
    }

    qDebug() << resp.getValueFromBody("create_time").toString();
    emit NeedUpdateTime(chat_id_, QDateTime::fromString(resp.getValueFromBody("create_time").toString(), "yyyy-MM-dd hh:mm:ss"));

    if(message_for_widget.files_.size() == 0){
        messages_in_chats_[chat_id_].messages->changeMessageInfo(pre_id,
                                                                 resp.getValueFromBody("id").toInt(),
                                                                 QDateTime::fromString(resp.getValueFromBody("create_time").toString(), "yyyy-MM-dd hh:mm:ss").toString("dd.MM.yy HH:mm"),
                                                                 1);
    }
    else {
        messages_in_chats_[chat_id_].messages->changeMessageInfo(pre_id,
                                                                 resp.getValueFromBody("id").toInt(),
                                                                 QDateTime::fromString(resp.getValueFromBody("create_time").toString(), "yyyy-MM-dd hh:mm:ss").toString("dd.MM.yy HH:mm"),
                                                                 0);

        connect(upload_manager_worker_->GetFileManager(), &FileUploadManager::uploadProgress, message_w, &MessageWidget::uploadProgressSlot);
        connect(upload_manager_worker_->GetFileManager(), &FileUploadManager::uploadCompleted, message_w, &MessageWidget::uploadCompletedSlot);

        upload_manager_worker_->enqueueUpload(message_for_widget.chat_id_,
                                              resp.getValueFromBody("id").toInt(),
                                              message_for_widget.files_);
    }

    QDateTime date_time = QDateTime::fromString(resp.getValueFromBody("create_time").toString(), "yyyy-MM-dd hh:mm:ss");
    date_time.setTimeZone(QTimeZone::utc());
    data::GeneralData::GetInstance()->SetLastChatUpdateTime(chat_id_, date_time);

    message_edit->clear();
    input_panel_->ClearFiles();
    messages_in_chats_[chat_id_].messages->clearFiles();
    messages_in_chats_[chat_id_].messages->SetUnreadMessageLine();
}

void ChatWidget::OnChangeMessage(MessageWidget* message_widget){
    QTextEdit* message_edit = input_panel_->getMessageInput();
    message_edit->setText(message_widget->getMessage());
    QPushButton* send_button = input_panel_->getSendButton();
    send_button->setText("Изменить");
    disconnect(send_button, SIGNAL(clicked(bool)), input_panel_, SIGNAL(ClickSendMessage()));
    connect(send_button, SIGNAL(clicked(bool)), this, SLOT(ClickToChangeMessage()));
    changed_message_id = message_widget->getMessageId();

    input_panel_->ShowMessageInfo("Редактирование", message_widget);
    connect(input_panel_, SIGNAL(ClickToCancel()), this, SLOT(CancelChangeMessage()));
}

void ChatWidget::ClickToChangeMessage(){
    if(changed_message_id == std::nullopt){
        return;
    }

    QTextEdit* message_edit = input_panel_->getMessageInput();
    QString message = message_edit->toPlainText();
    if(message.isEmpty()){
        return;
    }

    Request req = req_resp_utils::MakeChangeMessageRequest(data::GeneralData::GetInstance()->GetToken(),
                                                         chat_id_,
                                                         changed_message_id.value(),
                                                         message);
    Response resp = req_resp_utils::SendReqAndWaitResp(req);
    if(resp.getStatus() != 200){
        utils::MakeMessageBox(resp.getValueFromBody("message").toString());
        return;
    }

    message_edit->clear();
    messages_in_chats_[chat_id_].messages->changeMessage(changed_message_id.value(), message);

    QPushButton* send_button = input_panel_->getSendButton();
    send_button->setText("Отправить");
    connect(send_button, SIGNAL(clicked(bool)), input_panel_, SIGNAL(ClickSendMessage()));
    disconnect(send_button, SIGNAL(clicked(bool)), this, SLOT(ClickToChangeMessage()));
    changed_message_id = std::nullopt;
    input_panel_->HideMessageInfo();
}

void ChatWidget::ChoseFile(QString filename){
    messages_in_chats_[chat_id_].messages->addChosenFile(filename);
}

void ChatWidget::CancelChangeMessage(){
    QTextEdit* message_edit = input_panel_->getMessageInput();
    message_edit->clear();
    QPushButton* send_button = input_panel_->getSendButton();
    send_button->setText("Отправить");
    connect(send_button, SIGNAL(clicked(bool)), input_panel_, SIGNAL(ClickSendMessage()));
    disconnect(send_button, SIGNAL(clicked(bool)), this, SLOT(ClickToChangeMessage()));
    changed_message_id = std::nullopt;
    input_panel_->HideMessageInfo();
}

void ChatWidget::ChangeToChat(entities::ChatInfo info, bool isDialog, const entities::MessagesToChat& messages){
    if(info.chat_id_ == chat_id_){
        return;
    }

    if(messages_in_chats_.find(info.chat_id_) == messages_in_chats_.end()){
        messages_in_chats_[info.chat_id_] = CreateChatWidgets(info, isDialog);
    }
    WidgetsToChat* chat = &messages_in_chats_[info.chat_id_];
    auto widgets_ = chat->messages->addMessages(messages.messages_);
    for(auto w : widgets_){
        connect(w, &MessageWidget::fileDownloadRequested,
                this, &ChatWidget::onFileDownloadRequested);
    }

    QVBoxLayout* parent_layout = qobject_cast<QVBoxLayout*>(layout());
    if (parent_layout) {
        int profile_index = parent_layout->indexOf(messages_in_chats_[chat_id_].profile);
        int messages_index = parent_layout->indexOf(messages_in_chats_[chat_id_].messages);
        WidgetsToChat* current_chat = &messages_in_chats_[chat_id_];
        if (profile_index != -1 && messages_index != -1) {
            parent_layout->removeWidget(current_chat->profile);
            parent_layout->removeWidget(current_chat->messages);
            current_chat->profile->hide();
            current_chat->messages->hide();
            parent_layout->insertWidget(profile_index, chat->profile);
            parent_layout->insertWidget(messages_index, chat->messages);
            parent_layout->setStretch(0, 0);
            parent_layout->setStretch(1, 1);
            chat->profile->show();
            chat->messages->show();
        }
    }

    chat_id_ = info.chat_id_;
    if(!chat->profile->isVisible()){
        chat->profile->show();
    }
    if(!chat->messages->isVisible()){
        chat->messages->show();
    }
}

void ChatWidget::UpdateCurrentChat(const entities::MessagesToChat& messages){
    WidgetsToChat* chat = &messages_in_chats_[messages.id_];
    for(const entities::Message& message : messages.messages_){
        chat->messages->addMessage(message);
    }
}

MessageWidget* ChatWidget::AddMessageToChat(entities::Message message){
    if(messages_in_chats_.find(message.chat_id_) == messages_in_chats_.end()){
        return nullptr;
    }
    WidgetsToChat* chat = &messages_in_chats_[message.chat_id_];
    auto message_w = chat->messages->addMessage(message);
    connect(message_w, &MessageWidget::fileDownloadRequested,
            this, &ChatWidget::onFileDownloadRequested);  

    return message_w;
}

MessageWidget* ChatWidget::AddPreMessageToChat(entities::Message message){
    if(messages_in_chats_.find(message.chat_id_) == messages_in_chats_.end()){
        return nullptr;
    }
    WidgetsToChat* chat = &messages_in_chats_[message.chat_id_];

    emit NeedUpdateTime(message.chat_id_, QDateTime::fromString(message.create_time_));

    return chat->messages->addPreMessage(message);
}

void ChatWidget::DeleteMessageToChat(int chat_id, int message_id){
    if(messages_in_chats_.find(chat_id) == messages_in_chats_.end()){
        return;
    }
    WidgetsToChat* chat = &messages_in_chats_[chat_id];
    chat->messages->deleteMessage(message_id);
    // if(messages_in_chats_[chat_id].messages.
}

void ChatWidget::UpdateStatusToMessage(entities::Status status){
    if(messages_in_chats_.find(status.chat_id_) == messages_in_chats_.end()){
        return;
    }
    WidgetsToChat* chat = &messages_in_chats_[status.chat_id_];
    chat->messages->updateMessageStatus(status);
}

void ChatWidget::ChangeMessage(int chat_id, int message_id, QString text){
    if(messages_in_chats_.find(chat_id) == messages_in_chats_.end()){
        return;
    }
    WidgetsToChat* chat = &messages_in_chats_[chat_id];
    chat->messages->changeMessage(message_id, text);
}

void ChatWidget::DeleteChat(int chat_id) {
    if(messages_in_chats_.find(chat_id) != messages_in_chats_.end()) {
        HideChat();
        messages_in_chats_.erase(chat_id);
    }
}

void ChatWidget::onFileDownloadRequested(int chatId, int messageId, int fileIndex, QString filename) {
    if(messages_in_chats_.find(chatId) == messages_in_chats_.end()) return;

    connect(download_manager_worker_->GetFileManager(), &FileDownloadManager::downloadProgress,
    [this, chat_id__ = chatId](int messageId, int fileIndex, qint64 bytesReceived, qint64 total) {
        messages_in_chats_[chat_id__].messages->getById(messageId)->fileDownloadProgressSlot(messageId, fileIndex, bytesReceived, total);
    });
    connect(download_manager_worker_->GetFileManager(), &FileDownloadManager::downloadCompleted,
    [this, chat_id__ = chatId](int messageId, int fileIndex){
        messages_in_chats_[chat_id__].messages->getById(messageId)->fileDownloadCompletedSlot(messageId, fileIndex);
    });

    download_manager_worker_->enqueueDownload(messageId,
                                              fileIndex,
                                              filename);
}



void ChatWidget::StartCall(int chat_id, QString chat_name) {
    if (chat_id < 0) {
        return;
    }

    active_call_chat_id_ = chat_id;
    call_signaling_role_ = CallSignalingRole::Outgoing;
    if (!call_session_->startOutgoing()) {
        call_signaling_role_ = CallSignalingRole::None;
        return;
    }

    Request req;
    req.setPath("start_call");
    req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
    req.setValueToBody("chat_id", chat_id);
    req_resp_utils::SendReqAndWaitResp(req);

    if (outgoing_call_dialog_) {
        outgoing_call_dialog_->close();
        outgoing_call_dialog_->deleteLater();
        outgoing_call_dialog_ = nullptr;
    }

    outgoing_call_dialog_ = new QDialog(nullptr);
    outgoing_call_dialog_->setWindowTitle(QString("Звонок: %1").arg(chat_name));
    outgoing_call_dialog_->setWindowModality(Qt::ApplicationModal);
    QVBoxLayout* layout = new QVBoxLayout(outgoing_call_dialog_);
    layout->addWidget(new QLabel("Ожидание подключения собеседника..."));
    QPushButton* cancel = new QPushButton("Отменить", outgoing_call_dialog_);
    layout->addWidget(cancel);
    connect(cancel, &QPushButton::clicked, this, [this, chat_id]() {
        Request decline;
        decline.setPath("end_call");
        decline.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
        decline.setValueToBody("chat_id", chat_id);
        req_resp_utils::SendReqAndWaitResp(decline);
        call_session_->close();
        active_call_chat_id_ = -1;
        call_signaling_role_ = CallSignalingRole::None;
        CloseCallWindows();
    });
    outgoing_call_dialog_->show();
    outgoing_call_dialog_->raise();
    outgoing_call_dialog_->activateWindow();
}

void ChatWidget::OnIncomingCall(int chat_id, int caller_id, QString caller_name, QString offer_sdp) {
    if (caller_id == data::GeneralData::GetInstance()->GetUserId()) {
        return;
    }

    if (active_call_chat_id_ == chat_id && call_signaling_role_ == CallSignalingRole::Outgoing) {
        return;
    }

    pending_incoming_chat_id_ = chat_id;
    pending_incoming_offer_ = offer_sdp;
    pending_incoming_candidates_.clear();

    if (incoming_call_dialog_) {
        incoming_call_dialog_->close();
        incoming_call_dialog_->deleteLater();
        incoming_call_dialog_ = nullptr;
    }

    incoming_call_dialog_ = new QDialog(nullptr);
    incoming_call_dialog_->setWindowTitle("Входящий звонок");
    incoming_call_dialog_->setWindowModality(Qt::ApplicationModal);
    QVBoxLayout* layout = new QVBoxLayout(incoming_call_dialog_);
    layout->addWidget(new QLabel(QString("Входящий звонок от %1").arg(caller_name)));

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No, incoming_call_dialog_);
    box->button(QDialogButtonBox::Yes)->setText("Принять");
    box->button(QDialogButtonBox::No)->setText("Отклонить");
    layout->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, this, [this, caller_name]() {
        active_call_chat_id_ = pending_incoming_chat_id_;
        call_signaling_role_ = CallSignalingRole::Incoming;
        if (!call_session_->startIncoming(pending_incoming_offer_)) {
            active_call_chat_id_ = -1;
            call_signaling_role_ = CallSignalingRole::None;
            utils::MakeMessageBox("Не удалось инициализировать входящий звонок");
            return;
        }

        for (const auto& pendingCandidate : pending_incoming_candidates_) {
            call_session_->addRemoteCandidate(std::get<0>(pendingCandidate),
                                              std::get<1>(pendingCandidate),
                                              std::get<2>(pendingCandidate));
        }
        pending_incoming_candidates_.clear();

        Request accept;
        accept.setPath("accept_call");
        accept.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
        accept.setValueToBody("chat_id", pending_incoming_chat_id_);
        const Response acceptResp = req_resp_utils::SendReqAndWaitResp(accept);
        if (acceptResp.getStatus() != 200) {
            call_session_->close();
            active_call_chat_id_ = -1;
            call_signaling_role_ = CallSignalingRole::None;
            utils::MakeMessageBox(acceptResp.getValueFromBody("message").toString());
            return;
        }

        if (incoming_call_dialog_) {
            incoming_call_dialog_->close();
        }
        OpenActiveCallWindow(QString("Разговор с %1").arg(caller_name));
    });

    connect(box, &QDialogButtonBox::rejected, this, [this]() {
        Request decline;
        decline.setPath("decline_call");
        decline.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
        decline.setValueToBody("chat_id", pending_incoming_chat_id_);
        req_resp_utils::SendReqAndWaitResp(decline);
        if (incoming_call_dialog_) {
            incoming_call_dialog_->close();
        }
    });

    incoming_call_dialog_->show();
    incoming_call_dialog_->raise();
    incoming_call_dialog_->activateWindow();
}

void ChatWidget::OnCallAccepted(int chat_id, QString answer_sdp) {
    if (chat_id != active_call_chat_id_) {
        return;
    }
    call_signaling_role_ = CallSignalingRole::Outgoing;
    call_session_->applyRemoteAnswer(answer_sdp);
    if (outgoing_call_dialog_) {
        outgoing_call_dialog_->close();
    }
    OpenActiveCallWindow("Разговор");
}

void ChatWidget::OnCallDeclined(int chat_id) {
    if (chat_id != active_call_chat_id_) {
        return;
    }
    call_session_->close();
    active_call_chat_id_ = -1;
    call_signaling_role_ = CallSignalingRole::None;
    CloseCallWindows();
    utils::MakeMessageBox("Собеседник отклонил звонок");
}

void ChatWidget::OnCallEnded(int chat_id) {
    if (chat_id != active_call_chat_id_) {
        return;
    }
    call_session_->close();
    active_call_chat_id_ = -1;
    call_signaling_role_ = CallSignalingRole::None;
    CloseCallWindows();
}

void ChatWidget::OnRemoteCandidate(int chat_id, QString candidate, QString mid, int mline_index) {
    if (chat_id == active_call_chat_id_) {
        call_session_->addRemoteCandidate(candidate, mid, mline_index);
        return;
    }

    if (chat_id == pending_incoming_chat_id_ && active_call_chat_id_ < 0) {
        pending_incoming_candidates_.push_back(std::make_tuple(candidate, mid, mline_index));
    }
}

void ChatWidget::SendOffer(QString sdp) {
    if (active_call_chat_id_ < 0 || call_signaling_role_ != CallSignalingRole::Outgoing) {
        return;
    }
    Request req;
    req.setPath("call_offer");
    req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
    req.setValueToBody("chat_id", active_call_chat_id_);
    req.setValueToBody("sdp", sdp);
    req_resp_utils::SendReqAndWaitResp(req);
}

void ChatWidget::SendAnswer(QString sdp) {
    if (active_call_chat_id_ < 0 || call_signaling_role_ != CallSignalingRole::Incoming) {
        return;
    }
    Request req;
    req.setPath("call_answer");
    req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
    req.setValueToBody("chat_id", active_call_chat_id_);
    req.setValueToBody("sdp", sdp);
    req_resp_utils::SendReqAndWaitResp(req);
}

void ChatWidget::SendCandidate(QString candidate, QString mid, int mline_index) {
    if (active_call_chat_id_ < 0) {
        return;
    }
    Request req;
    req.setPath("call_candidate");
    req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
    req.setValueToBody("chat_id", active_call_chat_id_);
    req.setValueToBody("candidate", candidate);
    req.setValueToBody("mid", mid);
    req.setValueToBody("mline_index", mline_index);
    req_resp_utils::SendReqAndWaitResp(req);
}

void ChatWidget::OpenActiveCallWindow(const QString& titleText) {
    if (active_call_dialog_) {
        active_call_dialog_->raise();
        active_call_dialog_->activateWindow();
        return;
    }

    active_call_dialog_ = new QDialog(nullptr);
    active_call_dialog_->setAttribute(Qt::WA_DeleteOnClose, false);
    active_call_dialog_->setWindowTitle(titleText);
    active_call_dialog_->resize(900, 600);

    QVBoxLayout* root = new QVBoxLayout(active_call_dialog_);
    QLabel* callTitle = new QLabel(titleText, active_call_dialog_);
    callTitle->setStyleSheet("font-size: 18px; font-weight: 700;");
    root->addWidget(callTitle);

    QHBoxLayout* streamsLayout = new QHBoxLayout();
    remote_stream_label_ = new QLabel("Ожидание удалённого видео...", active_call_dialog_);
    remote_stream_label_->setAlignment(Qt::AlignCenter);
    remote_stream_label_->setMinimumSize(560, 320);
    remote_stream_label_->setStyleSheet("background-color: #111827; border: 1px solid #374151; border-radius: 12px;");

    local_stream_label_ = new QLabel("Локальное видео", active_call_dialog_);
    local_stream_label_->setAlignment(Qt::AlignCenter);
    local_stream_label_->setMinimumSize(220, 160);
    local_stream_label_->setStyleSheet("background-color: #1f2937; border: 1px solid #4b5563; border-radius: 12px;");

    streamsLayout->addWidget(remote_stream_label_, 3);
    streamsLayout->addWidget(local_stream_label_, 1);
    root->addLayout(streamsLayout, 1);

    QHBoxLayout* deviceSelectors = new QHBoxLayout();
    mic_device_combo_ = new QComboBox(active_call_dialog_);
    speaker_device_combo_ = new QComboBox(active_call_dialog_);
    camera_device_combo_ = new QComboBox(active_call_dialog_);

    deviceSelectors->addWidget(new QLabel("Микрофон:", active_call_dialog_));
    deviceSelectors->addWidget(mic_device_combo_, 1);
    deviceSelectors->addWidget(new QLabel("Динамик:", active_call_dialog_));
    deviceSelectors->addWidget(speaker_device_combo_, 1);
    deviceSelectors->addWidget(new QLabel("Камера:", active_call_dialog_));
    deviceSelectors->addWidget(camera_device_combo_, 1);
    root->addLayout(deviceSelectors);

    PopulateCallDeviceSelectors();

    connect(mic_device_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        selected_input_device_id_ = mic_device_combo_->currentData().toByteArray();
        StopMediaPipeline();
        StartMediaPipeline();
    });

    connect(speaker_device_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        selected_output_device_id_ = speaker_device_combo_->currentData().toByteArray();
        StopMediaPipeline();
        StartMediaPipeline();
    });

    connect(camera_device_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        selected_camera_device_id_ = camera_device_combo_->currentData().toByteArray();
        StopMediaPipeline();
        StartMediaPipeline();
    });

    QHBoxLayout* controls = new QHBoxLayout();
    toggle_mic_button_ = new QPushButton(active_call_dialog_);
    toggle_camera_button_ = new QPushButton(active_call_dialog_);
    QPushButton* endCallButton = new QPushButton("Завершить звонок", active_call_dialog_);
    endCallButton->setStyleSheet("background-color: #dc2626; color: white; border-radius: 8px; padding: 8px 14px;");

    controls->addWidget(toggle_mic_button_);
    controls->addWidget(toggle_camera_button_);
    controls->addStretch();
    controls->addWidget(endCallButton);
    root->addLayout(controls);

    connect(toggle_mic_button_, &QPushButton::clicked, this, [this]() {
        microphone_enabled_ = !microphone_enabled_;
        UpdateMediaControls();
    });
    connect(toggle_camera_button_, &QPushButton::clicked, this, [this]() {
        camera_enabled_ = !camera_enabled_;
        UpdateMediaControls();
    });
    connect(endCallButton, &QPushButton::clicked, this, [this]() {
        if (active_call_chat_id_ < 0) {
            CloseCallWindows();
            return;
        }
        Request endCallReq;
        endCallReq.setPath("end_call");
        endCallReq.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());
        endCallReq.setValueToBody("chat_id", active_call_chat_id_);
        req_resp_utils::SendReqAndWaitResp(endCallReq);

        call_session_->close();
        active_call_chat_id_ = -1;
        call_signaling_role_ = CallSignalingRole::None;
        CloseCallWindows();
    });

    UpdateMediaControls();
    StartMediaPipeline();
    active_call_dialog_->show();
    active_call_dialog_->raise();
    active_call_dialog_->activateWindow();
}


void ChatWidget::PopulateCallDeviceSelectors() {
    if (!mic_device_combo_ || !speaker_device_combo_ || !camera_device_combo_) {
        return;
    }

    QSignalBlocker micBlocker(mic_device_combo_);
    QSignalBlocker speakerBlocker(speaker_device_combo_);
    QSignalBlocker cameraBlocker(camera_device_combo_);

    mic_device_combo_->clear();
    const auto inputDevices = QMediaDevices::audioInputs();
    for (const QAudioDevice& device : inputDevices) {
        mic_device_combo_->addItem(device.description(), device.id());
    }

    speaker_device_combo_->clear();
    const auto outputDevices = QMediaDevices::audioOutputs();
    for (const QAudioDevice& device : outputDevices) {
        speaker_device_combo_->addItem(device.description(), device.id());
    }

    camera_device_combo_->clear();
    const auto cameraDevices = QMediaDevices::videoInputs();
    for (const QCameraDevice& device : cameraDevices) {
        camera_device_combo_->addItem(device.description(), device.id());
    }

    if (selected_input_device_id_.isEmpty()) {
        selected_input_device_id_ = QMediaDevices::defaultAudioInput().id();
    }
    if (selected_output_device_id_.isEmpty()) {
        selected_output_device_id_ = QMediaDevices::defaultAudioOutput().id();
    }
    if (selected_camera_device_id_.isEmpty() && !cameraDevices.isEmpty()) {
        selected_camera_device_id_ = cameraDevices.first().id();
    }

    const int micIndex = mic_device_combo_->findData(selected_input_device_id_);
    if (micIndex >= 0) {
        mic_device_combo_->setCurrentIndex(micIndex);
    }
    const int speakerIndex = speaker_device_combo_->findData(selected_output_device_id_);
    if (speakerIndex >= 0) {
        speaker_device_combo_->setCurrentIndex(speakerIndex);
    }
    const int cameraIndex = camera_device_combo_->findData(selected_camera_device_id_);
    if (cameraIndex >= 0) {
        camera_device_combo_->setCurrentIndex(cameraIndex);
    }
}

void ChatWidget::CloseCallWindows() {
    StopMediaPipeline();

    if (outgoing_call_dialog_) {
        outgoing_call_dialog_->close();
        outgoing_call_dialog_->deleteLater();
        outgoing_call_dialog_ = nullptr;
    }
    if (incoming_call_dialog_) {
        incoming_call_dialog_->close();
        incoming_call_dialog_->deleteLater();
        incoming_call_dialog_ = nullptr;
    }
    if (active_call_dialog_) {
        active_call_dialog_->close();
        active_call_dialog_->deleteLater();
        active_call_dialog_ = nullptr;
    }

    toggle_mic_button_ = nullptr;
    toggle_camera_button_ = nullptr;
    local_stream_label_ = nullptr;
    remote_stream_label_ = nullptr;
    mic_device_combo_ = nullptr;
    speaker_device_combo_ = nullptr;
    camera_device_combo_ = nullptr;
    microphone_enabled_ = true;
    camera_enabled_ = true;
    pending_incoming_chat_id_ = -1;
    pending_incoming_offer_.clear();
    pending_incoming_candidates_.clear();
    call_signaling_role_ = CallSignalingRole::None;
}

void ChatWidget::UpdateMediaControls() {
    if (toggle_mic_button_) {
        toggle_mic_button_->setText(microphone_enabled_ ? "Микрофон: включён" : "Микрофон: выключен");
    }
    if (toggle_camera_button_) {
        toggle_camera_button_->setText(camera_enabled_ ? "Камера: включена" : "Камера: выключена");
    }

    if (local_stream_label_ && !camera_enabled_) {
        local_stream_label_->setText("Камера отключена");
        local_stream_label_->setPixmap(QPixmap());
    }
}

void ChatWidget::StartMediaPipeline() {
    if (!camera_) {
        const QCameraDevice selectedCamera = ResolveCameraDevice(selected_camera_device_id_);
        if (!selectedCamera.isNull()) {
            selected_camera_device_id_ = selectedCamera.id();
            camera_ = new QCamera(selectedCamera, this);
            capture_session_ = new QMediaCaptureSession(this);
            image_capture_ = new QImageCapture(this);
            capture_session_->setCamera(camera_);
            capture_session_->setImageCapture(image_capture_);
            connect(image_capture_, &QImageCapture::imageCaptured, this, &ChatWidget::OnLocalFrameCaptured);
            camera_->start();

            video_capture_timer_ = new QTimer(this);
            connect(video_capture_timer_, &QTimer::timeout, this, [this]() {
                if (camera_enabled_ && image_capture_ && image_capture_->isReadyForCapture()) {
                    image_capture_->capture();
                }
            });
            video_capture_timer_->start(66);
        }
    }

    if (!audio_source_) {
        const QAudioDevice inputDevice = ResolveAudioInputDevice(selected_input_device_id_);
        const QAudioDevice outputDevice = ResolveAudioOutputDevice(selected_output_device_id_);

        selected_input_device_id_ = inputDevice.id();
        selected_output_device_id_ = outputDevice.id();

        audio_input_format_ = BuildVoiceAudioFormat(inputDevice);
        audio_output_format_ = BuildVoiceAudioFormat(outputDevice);

        if (inputDevice.isNull() || outputDevice.isNull()) {
            utils::MakeMessageBox("Не удалось найти аудио-устройство. Проверьте настройки микрофона/динамика.");
            return;
        }

        audio_source_ = new QAudioSource(inputDevice, audio_input_format_, this);
        audio_input_device_ = audio_source_->start();

        audio_sink_ = new QAudioSink(outputDevice, audio_output_format_, this);
        audio_output_device_ = audio_sink_->start();

        if (!audio_input_device_ || !audio_output_device_) {
            utils::MakeMessageBox("Не удалось запустить аудио-поток для звонка.");
            StopMediaPipeline();
            return;
        }

        audio_poll_timer_ = new QTimer(this);
        connect(audio_poll_timer_, &QTimer::timeout, this, &ChatWidget::SendAudioFrame);
        audio_poll_timer_->start(30);
    }
}

void ChatWidget::StopMediaPipeline() {
    if (video_capture_timer_) {
        video_capture_timer_->stop();
        video_capture_timer_->deleteLater();
        video_capture_timer_ = nullptr;
    }
    if (image_capture_) {
        image_capture_->deleteLater();
        image_capture_ = nullptr;
    }
    if (capture_session_) {
        capture_session_->deleteLater();
        capture_session_ = nullptr;
    }
    if (camera_) {
        camera_->stop();
        camera_->deleteLater();
        camera_ = nullptr;
    }

    if (audio_poll_timer_) {
        audio_poll_timer_->stop();
        audio_poll_timer_->deleteLater();
        audio_poll_timer_ = nullptr;
    }
    if (audio_source_) {
        audio_source_->stop();
        audio_source_->deleteLater();
        audio_source_ = nullptr;
        audio_input_device_ = nullptr;
    }
    if (audio_sink_) {
        audio_sink_->stop();
        audio_sink_->deleteLater();
        audio_sink_ = nullptr;
        audio_output_device_ = nullptr;
    }

    audio_input_format_ = QAudioFormat();
    audio_output_format_ = QAudioFormat();
}

void ChatWidget::OnLocalFrameCaptured(int id, const QImage& preview) {
    Q_UNUSED(id);
    if (!camera_enabled_ || preview.isNull()) {
        return;
    }

    const QImage scaled = preview.scaled(320, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (local_stream_label_) {
        local_stream_label_->setPixmap(QPixmap::fromImage(scaled));
        local_stream_label_->setText(QString());
    }

    SendVideoFrame(scaled);
}

void ChatWidget::SendVideoFrame(const QImage& frame) {
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    frame.save(&buffer, "JPEG", 55);

    QByteArray packet;
    packet.reserve(jpegData.size() + 1 + static_cast<int>(sizeof(quint32)));
    packet.append(static_cast<char>(0x01));
    AppendSenderId(packet, local_media_sender_id_);
    packet.append(jpegData);
    call_session_->sendMediaPacket(packet);
}

void ChatWidget::SendAudioFrame() {
    if (!microphone_enabled_ || !audio_input_device_) {
        return;
    }

    const QByteArray pcmChunk = audio_input_device_->readAll();
    if (pcmChunk.isEmpty()) {
        return;
    }

    const QByteArray transportPcm = ConvertAudioToTransport(pcmChunk, audio_input_format_);
    if (transportPcm.isEmpty()) {
        return;
    }

    QByteArray packet;
    packet.reserve(transportPcm.size() + 1 + static_cast<int>(sizeof(quint32)));
    packet.append(static_cast<char>(0x02));
    AppendSenderId(packet, local_media_sender_id_);
    packet.append(transportPcm);
    call_session_->sendMediaPacket(packet);
}

void ChatWidget::OnMediaPacketReceived(const QByteArray& packet) {
    if (packet.isEmpty()) {
        return;
    }

    const quint8 packetType = static_cast<quint8>(packet.at(0));
    QByteArray payload = packet.mid(1);

    quint32 senderId = 0;
    const bool hasSenderId = ExtractSenderId(payload, &senderId);
    if (hasSenderId && senderId == local_media_sender_id_) {
        return;
    }

    if (packetType == 0x01) {
        QImage remoteFrame;
        remoteFrame.loadFromData(payload, "JPEG");
        if (!remoteFrame.isNull() && remote_stream_label_) {
            QImage scaled = remoteFrame.scaled(640, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            remote_stream_label_->setPixmap(QPixmap::fromImage(scaled));
            remote_stream_label_->setText(QString());
        }
    } else if (packetType == 0x02) {
        PlayRemoteAudio(payload);
    }
}

void ChatWidget::PlayRemoteAudio(const QByteArray& pcmData) {
    if (!audio_output_device_ || pcmData.isEmpty()) {
        return;
    }

    const QByteArray outputPcm = ConvertTransportToOutput(pcmData, audio_output_format_);
    if (outputPcm.isEmpty()) {
        return;
    }

    audio_output_device_->write(outputPcm);
}

void ChatWidget::HideChat(){
    WidgetsToChat* temp_chat = &messages_in_chats_[-1];
    QVBoxLayout* parent_layout = qobject_cast<QVBoxLayout*>(layout());
    if (parent_layout) {
        int profile_index = parent_layout->indexOf(messages_in_chats_[chat_id_].profile);
        int messages_index = parent_layout->indexOf(messages_in_chats_[chat_id_].messages);
        WidgetsToChat* current_chat = &messages_in_chats_[chat_id_];
        if (profile_index != -1 && messages_index != -1) {
            parent_layout->removeWidget(current_chat->profile);
            parent_layout->removeWidget(current_chat->messages);
            current_chat->profile->hide();
            current_chat->messages->hide();
            parent_layout->insertWidget(profile_index, temp_chat->profile);
            parent_layout->insertWidget(messages_index, temp_chat->messages);
            parent_layout->setStretch(0, 0);
            parent_layout->setStretch(1, 1);
            temp_chat->profile->show();
            temp_chat->messages->show();
        }
    }
    chat_id_ = -1;
}

WidgetsToChat ChatWidget::CreateChatWidgets(entities::ChatInfo info, bool is_dialog){
    ChatHeader* profile = new ChatHeader(info.chat_id_, info.name_, info.time_, this);
    connect(profile, SIGNAL(clicked(int,QString)), this, SIGNAL(ChatDetailsClick(int,QString)));
    connect(profile, SIGNAL(startCallClicked(int,QString)), this, SLOT(StartCall(int,QString)));
    MessagesWidget* messages = new MessagesWidget(info.chat_id_, is_dialog, this);
    connect(messages, SIGNAL(OnChangeMessage(MessageWidget*)), this, SLOT(OnChangeMessage(MessageWidget*)));
    return {profile, messages};
}
