#ifndef MAINPAGE_H
#define MAINPAGE_H

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>

#include "entities.h"
#include "chatwidget.h"
#include "messagehandler.h"
#include "chatsbox.h"
#include "chatdetails.h"

namespace Ui {
class MainPage;
}

class MainPage : public QWidget
{
    Q_OBJECT

public:
    explicit MainPage(
        MessageHandler* handler,
        QString url,
        QWidget *parent = nullptr);

    ~MainPage();

    void showEvent(QShowEvent* event) override;
    void setLogRegPage(QWidget* login_widget);

    void ResetState();

public slots:
    void ShowCreateChat();
    void ClickToChat(Chat*);
    void ChatDelatilsClicked(int chat_id, QString chat_name);
    void UpdateChatTime(int chat_id, QDateTime time);

private slots:
    void NewChat(entities::Chat);
    void AddChatMember(entities::UserInfoInChat);
    void ChangeMemberRole(entities::ChatMember);
    void NewMessage(entities::Message);
    void DeleteMessage(int chat_id, int message_id);
    void UpdateMessageStatus(entities::Status);
    void ChangeMessage(int chat_id, int message_id, QString text);
    void DeleteChatMember(int chat_id, int member_id);
    void BackDetailsClick();
    void OnContentClick(int message_id);

    void OnLogout();

private:
    void Prepare();
    void FillPage();
    void FillChats();
    QWidget* CreateWidgetWithoutChat();

    std::shared_ptr<UploadManagerWorker> upload_manager_worker_;
    ChatsBox* chats_box_;
    QWidget* without_chat_;
    ChatWidget* chat_widgets_ = nullptr;
    std::map<int, ChatDetails*> chat_details_;
    int current_chat_ = -1;

    QWidget* login_widget_;
    QPushButton* logout_btn_;

    Ui::MainPage *ui;
    bool is_fill_ = false;
};

#endif
