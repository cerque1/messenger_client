#pragma once
#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QList>
#include <QByteArray>

#include <mutex>

class QTextEdit;
class QPushButton;
class QDialog;
class QLabel;
class QCamera;
class QMediaCaptureSession;
class QImageCapture;
class QAudioSource;
class QAudioSink;
class QIODevice;
class QTimer;
class QImage;

#include "entities.h"
#include "chatheader.h"
#include "filesdisplaywidget.h"
#include "fileitemwidget.h"
#include "uploadmanagerworker.h"
#include "downloadmanagerworker.h"
#include "callsession.h"

class ChatWidget;

class MessageWidget : public QWidget{
    Q_OBJECT
public:
    explicit MessageWidget(const entities::Message& message, bool isNew, QWidget* parent);
    virtual ~MessageWidget() = default;

    void setMessage(QString message);
    void setTime(QString time);
    void setId(int id){
        message_id_ = id;
        files_list_->setMessageId(id);
    }

    int getMessageId() const{
        return message_id_;
    }
    int getCurrentStatus() const{
        return current_status_;
    }
    int getSenderId() const{
        return sender_id_;
    }
    QString getMessage() const {
        return findChild<QLabel*>("message_label")->text();
    }

    void changeMessage(QString text){
        QLabel* label = findChild<QLabel*>("message_label");
        label->setText(text);

        if(!is_changed_){
            is_changed_label_->show();
            is_changed_ = true;

            message_block_layout->addStretch();
            message_block_layout->activate();
            message_block_layout->update();
        }

        adjustSize();
        updateGeometry();
        update();

        emit onChangeMessage();
    }


    virtual void setStatus(int status) = 0;

signals:
    void deleteAction(bool);
    void changeMessageSignal();
    void onChangeMessage();
    void fileDownloadRequested(int chatId, int messageId, int fileIndex, QString filename);

    void SetProgress(int messageId, int fileIndex, qint64 uploaded, qint64 total);
    void SetComplete(int messageId, int fileIndex, int newContentId);

public slots:
    void uploadProgressSlot(int messageId, int fileIndex, qint64 uploaded, qint64 total);
    void uploadCompletedSlot(int messageId, int fileIndex, int newContentId);

    void fileDownloadProgressSlot(int messageId, int fileIndex, qint64 bytesReceived, qint64 total);
    void fileDownloadCompletedSlot(int messageId, int fileIndex);

    void onFileDownloadRequested(int messageId, int fileIndex, QString filename);

private slots:
    void deleteActionSlot();

protected:
    int chat_id_;
    int message_id_;
    int current_status_;
    int current_time_;
    int sender_id_;
    bool is_changed_;

    QVBoxLayout* message_block_layout;
    QLabel* is_changed_label_;
    FilesListWidget* files_list_;
};

class OwnMessage : public MessageWidget{
    Q_OBJECT
public:
    explicit OwnMessage(const entities::Message& message, bool isNew, QWidget* parent);

    void setStatus(int status) override;

private slots:
    void showContextMenuSlot(const QPoint &pos);
    void fullDeleteActionSlot();
    void updateActionSlot();
};

class InterlocutorMessage : public MessageWidget{
    Q_OBJECT
public:
    explicit InterlocutorMessage(const entities::Message& message, bool is_dialog, QWidget* parent);

    void setStatus(int status) override;

private slots:
    void showContextMenuSlot(const QPoint &pos);
};


class MessagesWidget : public QScrollArea{
    Q_OBJECT
public:
    explicit MessagesWidget(int chat_id, bool is_dialog, QWidget *parent = nullptr);

    QList<MessageWidget*> addMessages(const QList<entities::Message>& messages);
    MessageWidget* addMessage(const entities::Message& message);
    MessageWidget* addPreMessage(const entities::Message& message);
    void deleteMessage(int message_id);
    void changeMessageInfo(int pre_id, int id, QString time, int status);
    void updateMessageStatus(const entities::Status& status);
    void changeMessage(int message_id, QString text);
    void addChosenFile(QString filename){
        chosen_files_.push_back(filename);
    }
    int getContentCount() const {
        return chosen_files_.size();
    }
    QList<QString> getFiles() const{
        return chosen_files_;
    }
    MessageWidget* getById(int message_id) {
        return messages_[message_id];
    }
    void clearFiles(){
        chosen_files_.clear();
    }

    int SetUnreadMessageLine();

    void OnContentClick(int message_id);

signals:
    void OnChangeMessage(MessageWidget*);

private slots:
    void ValueChanged(int value);
    void OnDeleteMessageAction(bool full_delete);
    void OnChangeMessageSlot(){
        emit OnChangeMessage(qobject_cast<MessageWidget*>(sender()));
    }

private:
    std::mutex mu;

    MessageWidget* CreateMessageWidget(entities::Message message, bool isNew);

    QMap<int, MessageWidget*> messages_;
    QMap<int, MessageWidget*> pre_messages_;
    QList<int> unread_messages_;
    std::optional<int> first_unread_message_ = std::nullopt;
    QLabel* unread_message_line_ = nullptr;
    int chat_id_;
    bool is_dialog_;
    int last_id_ = -1;
    QList<QString> chosen_files_;
};

class MessageInfoWidget : public QWidget{
    Q_OBJECT
public:
    explicit MessageInfoWidget(QWidget* parent = nullptr);

    void SetMessageHeader(QString header);
    void SetMessage(MessageWidget* message);
    void Clear();

signals:
    void CloseMessageInfo();

private slots:
    void OnMessageUpdate();

private:
    MessageWidget* message_;
    QLabel* header_;
    QLabel* message_content_;
};

class InputPanelWidget : public QWidget{
    Q_OBJECT
public:
    explicit InputPanelWidget(QWidget *parent = nullptr);

    void ShowMessageInfo(QString header, MessageWidget* message);
    void HideMessageInfo();
    void AddFileName(const QString& fileName);
    void ClearFiles();
    
    QTextEdit* getMessageInput() const { return message_input_; }
    QPushButton* getSendButton() const { return send_button_; }

protected:
    void resizeEvent(QResizeEvent* event) override;

public slots:
    void ClickToCancelSlot();
    void ClickToFileButtonSlot();
    void OnFileRemoved(const QString& fileName);
    void adjustInputHeight();

signals:
    void ClickSendMessage();
    void ClickToCancel();
    void ChoseFile(QString);
    void RemoveFile(const QString& fileName);

private:
    MessageInfoWidget* message_info_;
    FilesDisplayWidget* files_display_;
    QTextEdit* message_input_;
    QPushButton* send_button_;
};

struct WidgetsToChat{
    ChatHeader* profile;
    MessagesWidget* messages;
};

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(int chat_id, std::shared_ptr<UploadManagerWorker> upload_manager_worker, QWidget *parent = nullptr);

    void ChangeToChat(entities::ChatInfo info, bool isDialog, const entities::MessagesToChat& messages);
    void UpdateCurrentChat(const entities::MessagesToChat& messages);
    MessageWidget* AddMessageToChat(entities::Message message);
    MessageWidget* AddPreMessageToChat(entities::Message message);
    void DeleteMessageToChat(int chat_id, int message_id);
    void UpdateStatusToMessage(entities::Status status);
    void ChangeMessage(int chat_id, int message_id, QString text);
    void DeleteChat(int chat_id);

    void OnContentClick(int message_id);

signals:
    void ChatDetailsClick(int chat_id, QString chat_name);
    void NeedUpdateTime(int chat_id, QDateTime time);

public slots:
    void ClickToSendMessage();
    void OnChangeMessage(MessageWidget*);
    void ClickToChangeMessage();
    void CancelChangeMessage();
    void ChoseFile(QString);
    void onFileDownloadRequested(int chatId, int messageId, int fileIndex, QString filename);

private slots:
    void StartCall(int chat_id, QString chat_name);
    void OnIncomingCall(int chat_id, int caller_id, QString caller_name, QString offer_sdp);
    void OnCallAccepted(int chat_id, QString answer_sdp);
    void OnCallDeclined(int chat_id);
    void OnCallEnded(int chat_id);
    void OnRemoteCandidate(int chat_id, QString candidate, QString mid, int mline_index);
    void SendOffer(QString sdp);
    void SendAnswer(QString sdp);
    void SendCandidate(QString candidate, QString mid, int mline_index);
    void OnLocalFrameCaptured(int id, const QImage& preview);
    void OnMediaPacketReceived(const QByteArray& packet);
    void SendAudioFrame();

private:
    WidgetsToChat CreateChatWidgets(entities::ChatInfo info, bool is_dialog);
    enum class CallSignalingRole {
        None,
        Outgoing,
        Incoming
    };

    void HideChat();
    void OpenActiveCallWindow(const QString& titleText);
    void CloseCallWindows();
    void UpdateMediaControls();
    void StartMediaPipeline();
    void StopMediaPipeline();
    void SendVideoFrame(const QImage& frame);
    void PlayRemoteAudio(const QByteArray& pcmData);

    InputPanelWidget* input_panel_;

    int chat_id_;
    std::optional<int> changed_message_id = std::nullopt;
    std::unordered_map<int, WidgetsToChat> messages_in_chats_;
    std::shared_ptr<UploadManagerWorker> upload_manager_worker_;
    std::unique_ptr<DownloadManagerWorker> download_manager_worker_;

    std::unique_ptr<CallSession> call_session_;
    QDialog* outgoing_call_dialog_ = nullptr;
    QDialog* incoming_call_dialog_ = nullptr;
    QDialog* active_call_dialog_ = nullptr;
    QPushButton* toggle_mic_button_ = nullptr;
    QPushButton* toggle_camera_button_ = nullptr;
    QLabel* local_stream_label_ = nullptr;
    QLabel* remote_stream_label_ = nullptr;

    QCamera* camera_ = nullptr;
    QMediaCaptureSession* capture_session_ = nullptr;
    QImageCapture* image_capture_ = nullptr;
    QTimer* video_capture_timer_ = nullptr;
    QAudioSource* audio_source_ = nullptr;
    QIODevice* audio_input_device_ = nullptr;
    QAudioSink* audio_sink_ = nullptr;
    QIODevice* audio_output_device_ = nullptr;
    QTimer* audio_poll_timer_ = nullptr;

    bool microphone_enabled_ = true;
    bool camera_enabled_ = true;
    int active_call_chat_id_ = -1;
    int pending_incoming_chat_id_ = -1;
    QString pending_incoming_offer_;
    CallSignalingRole call_signaling_role_ = CallSignalingRole::None;
};

#endif // CHATWIDGET_H
