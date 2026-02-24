#include "mainpage.h"
#include "ui_mainpage.h"
#include "generaldata.h"
#include "req_resp_utils.h"
#include "createchat.h"
#include "utils.h"
#include "chatsbox.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QTabBar>
#include <QScrollArea>

MainPage::MainPage(
    MessageHandler* handler,
    QString url,
    QWidget *parent)
    : QWidget(parent)
    , upload_manager_worker_(new UploadManagerWorker(url))
    , ui(new Ui::MainPage)
{
    ui->setupUi(this);
    this->setStyleSheet(
        "QWidget#MainPage {"
        "    background-color: #0b0c0e;"
        "    color: #e6e6e6;"
        "    font-family: \"Segoe UI\", \"Inter\", sans-serif;"
        "    font-size: 14px;"
        "}");

    connect(handler, SIGNAL(NewChat(entities::Chat)), this, SLOT(NewChat(entities::Chat)));
    connect(handler, SIGNAL(NewMessage(entities::Message)), this, SLOT(NewMessage(entities::Message)));
    connect(handler, SIGNAL(ChangeMemberRole(entities::ChatMember)), this, SLOT(ChangeMemberRole(entities::ChatMember)));
    connect(handler, SIGNAL(UpdateMessageStatus(entities::Status)), this, SLOT(UpdateMessageStatus(entities::Status)));
    connect(handler, SIGNAL(DeleteMessage(int,int)), this, SLOT(DeleteMessage(int,int)));
    connect(handler, SIGNAL(ChangeMessage(int,int,QString)), this, SLOT(ChangeMessage(int,int,QString)));
    connect(handler, SIGNAL(DeleteChatMember(int,int)), this, SLOT(DeleteChatMember(int,int)));
    connect(handler, SIGNAL(AddChatMember(entities::UserInfoInChat)), this, SLOT(AddChatMember(entities::UserInfoInChat)));

    Prepare();

    connect(handler, SIGNAL(IncomingCall(int,int,QString,QString)), chat_widgets_, SLOT(OnIncomingCall(int,int,QString,QString)));
    connect(handler, SIGNAL(CallAccepted(int,QString)), chat_widgets_, SLOT(OnCallAccepted(int,QString)));
    connect(handler, SIGNAL(CallDeclined(int)), chat_widgets_, SLOT(OnCallDeclined(int)));
    connect(handler, SIGNAL(CallEnded(int)), chat_widgets_, SLOT(OnCallEnded(int)));
    connect(handler, SIGNAL(CallCandidate(int,QString,QString,int)), chat_widgets_, SLOT(OnRemoteCandidate(int,QString,QString,int)));
}

void MainPage::setLogRegPage(QWidget* login_widget){
    login_widget_ = login_widget;
}

void MainPage::Prepare()
{
    QGridLayout* grid_layout = ui->grid_layout;

    QScrollArea* chats_with_scroll = new QScrollArea(this);
    chats_with_scroll->setObjectName("scroll_for_chats");
    chats_with_scroll->setMinimumWidth(200);
    chats_with_scroll->setMaximumWidth(300);
    chats_with_scroll->setStyleSheet(
        "QScrollArea#scroll_for_chats {"
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

    QWidget* chats_widget = new QWidget(this);
    chats_widget->setMinimumWidth(200);
    chats_widget->setMaximumWidth(300);
    chats_widget->setObjectName("chats_widget");
    chats_widget->setStyleSheet(
        "QWidget#chats_widget {"
        "    background-color: #0b0c0e;"
        "}");

    chats_box_ = new ChatsBox(chats_widget);
    chats_box_->setContentsMargins(8, 8, 8, 8);
    chats_box_->setSpacing(4);
    chats_widget->setLayout(chats_box_);

    connect(chats_box_, SIGNAL(Click(Chat*)), this, SLOT(ClickToChat(Chat*)));
    connect(chats_box_, SIGNAL(DeleteChatForMe(int,int)), this, SLOT(DeleteChatMember(int,int)));

    chats_with_scroll->setWidget(chats_widget);
    chats_with_scroll->setWidgetResizable(true);

    logout_btn_ = new QPushButton("Выйти");
    logout_btn_->setMinimumHeight(35);
    logout_btn_->setMinimumWidth(200);
    logout_btn_->setMaximumWidth(300);
    logout_btn_->setStyleSheet(
        "QPushButton {"
        "    background-color: #2563eb;"
        "    border: 1px solid #2563eb;"
        "    border-radius: 10px;"
        "    color: white;"
        "    font-weight: 600;"
        "    padding: 7px 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #3b82f6;"
        "    border: 1px solid #3b82f6;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1d4ed8;"
        "    border: 1px solid #1d4ed8;"
        "}");
    connect(logout_btn_, &QPushButton::clicked, this, &MainPage::OnLogout);

    QWidget* left_panel = new QWidget;
    left_panel->setStyleSheet(
        "QWidget {"
        "    background-color: #0b0c0e;"
        "}");
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(8, 8, 8, 8);
    left_layout->setSpacing(8);
    left_layout->addWidget(chats_with_scroll);
    left_layout->addWidget(logout_btn_);

    without_chat_ = CreateWidgetWithoutChat();

    chat_widgets_ = new ChatWidget(-1, upload_manager_worker_, this);
    connect(chat_widgets_, SIGNAL(ChatDetailsClick(int,QString)), this, SLOT(ChatDelatilsClicked(int,QString)));
    connect(chat_widgets_, SIGNAL(NeedUpdateTime(int,QDateTime)), this, SLOT(UpdateChatTime(int,QDateTime)));
    chat_widgets_->hide();

    grid_layout->addWidget(left_panel, 0, 0, 1, 1);
    grid_layout->addWidget(without_chat_, 0, 1, 1, 3);

    grid_layout->setColumnStretch(1, 3);
    grid_layout->setColumnStretch(0, 1);
}

void MainPage::OnLogout()
{
    data::GeneralData::GetInstance()->Clear();

    this->hide();

    ResetState();

    if(login_widget_)
        login_widget_->show();
}

void MainPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);


    if(!chats_box_)
        Prepare();

    if(is_fill_)
        return;

    is_fill_ = true;
    FillPage();
}

void MainPage::FillPage()
{
    FillChats();

    QPushButton* button = new QPushButton("создать чат");
    button->setMinimumWidth(175);
    button->setMaximumWidth(300);
    button->setStyleSheet(
        "QPushButton {"
        "    background-color: #2563eb;"
        "    border: 1px solid #2563eb;"
        "    border-radius: 10px;"
        "    color: white;"
        "    font-weight: 600;"
        "    padding: 7px 16px;"
        "    margin: 4px 0px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #3b82f6;"
        "    border: 1px solid #3b82f6;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1d4ed8;"
        "    border: 1px solid #1d4ed8;"
        "}");

    connect(button, SIGNAL(clicked()), this, SLOT(ShowCreateChat()));
    chats_box_->addWidget(button);
}

void MainPage::FillChats()
{
    QDateTime last_update_time = QDateTime::currentDateTime();

    Request req = req_resp_utils::MakeGetChatRequest(
        data::GeneralData::GetInstance()->GetToken(),
        data::GeneralData::GetInstance()->GetLastUpdateTime().toUTC());

    Response resp = req_resp_utils::SendReqAndWaitResp(req);

    if(resp.getStatus() != 200){
        utils::MakeMessageBox(resp.getValueFromBody("message").toString());
        return;
    }

    for(const entities::Chat& chat : req_resp_utils::MakeChatFromResponse(resp)){
        chats_box_->AddOrUpdateChat(chat);
    }

    data::GeneralData::GetInstance()->SetLastUpdateTime(last_update_time);
}

void MainPage::ShowCreateChat()
{
    CreateChat* create_chat_window = new CreateChat();
    create_chat_window->show();
}

void MainPage::ClickToChat(Chat* chat_button)
{
    if(chat_button->objectName().toInt() == current_chat_)
        return;

    if(chat_details_[current_chat_] && chat_details_[current_chat_]->isVisible())
        chat_details_[current_chat_]->hide();

    QDateTime last_update_time = QDateTime::currentDateTime();

    Request req = req_resp_utils::MakeGetMessagesRequest(
        data::GeneralData::GetInstance()->GetToken(),
        chat_button->objectName().toInt(),
        data::GeneralData::GetInstance()->GetLastChatUpdateTime(chat_button->objectName().toInt()).toUTC());

    Response resp = req_resp_utils::SendReqAndWaitResp(req);

    if(resp.getStatus() != 200){
        utils::MakeMessageBox(resp.getValueFromBody("message").toString());
        return;
    }

    data::GeneralData::GetInstance()->SetLastChatUpdateTime(
        chat_button->objectName().toInt(),
        last_update_time);

    Request chats_members_req;
    chats_members_req.setPath("get_chat_members");
    chats_members_req.setValueToBody("chat_id", chat_button->objectName().toInt());
    chats_members_req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());

    Response chats_members_resp = req_resp_utils::SendReqAndWaitResp(chats_members_req);

    data::GeneralData::GetInstance()->AddChatMembersToChat(
        chat_button->objectName().toInt(),
        req_resp_utils::MakeChatMembersFromResponse(chats_members_resp));

    QStringList parts = chat_button->GetName().split('#');
    QString another_user =
        (parts[0] == data::GeneralData::GetInstance()->GetUserName())
            ? parts[1]
            : parts[0];

    chat_widgets_->ChangeToChat(
        {chat_button->objectName().toInt(), another_user, chat_button->GetLast()},
        chats_box_->getChatById(chat_button->objectName().toInt()).is_dialog_,
        req_resp_utils::MakeMessagesFromResponse(resp));

    current_chat_ = chat_button->objectName().toInt();

    if(without_chat_->isVisible()){
        without_chat_->hide();
        ui->grid_layout->addWidget(chat_widgets_, 0, 1, 1, 3);
    }

    if(!chat_widgets_->isVisible())
        chat_widgets_->show();
}

void MainPage::ChatDelatilsClicked(int chat_id, QString chat_name)
{
    if(chat_details_.find(chat_id) == chat_details_.end() || chat_details_[chat_id] == nullptr){
        chat_details_[chat_id] = new ChatDetails(
            chat_id,
            chat_name,
            chats_box_->getChatById(chat_id).is_dialog_,
            this);

        connect(chat_details_[chat_id], SIGNAL(BackClick()), this, SLOT(BackDetailsClick()));
        connect(chat_details_[chat_id], SIGNAL(ContentClicked(int)), this, SLOT(OnContentClick(int)));
    }

    chat_widgets_->hide();
    ui->grid_layout->addWidget(chat_details_[chat_id], 0, 1, 1, 3);
    chat_details_[chat_id]->show();
}

void MainPage::UpdateChatTime(int chat_id, QDateTime time){
    chats_box_->UpdateChat(chat_id, time.toString("yyyy-MM-ddTHH:mm:ss.zzz"));
}

void MainPage::BackDetailsClick()
{
    chat_details_[current_chat_]->hide();
    chat_widgets_->show();
}

void MainPage::OnContentClick(int message_id)
{
    chat_details_[current_chat_]->hide();
    chat_widgets_->show();
    chat_widgets_->OnContentClick(message_id);
}

void MainPage::NewChat(entities::Chat chat)
{
    chats_box_->AddOrUpdateChat(chat);
}

void MainPage::AddChatMember(entities::UserInfoInChat chat_member)
{
    auto chat_members = data::GeneralData::GetInstance()->getMembersToChat(chat_member.chat_id_);
    if(!chat_members)
        return;

    chat_members->users_[chat_member.chat_id_] = chat_member;

    if(chat_details_.find(chat_member.chat_id_) == chat_details_.end()
        || chat_details_[chat_member.chat_id_] == nullptr)
        return;

    chat_details_[chat_member.chat_id_]->addMemberToList(
        chat_member.name_,
        chat_member.id_,
        chat_member.role_);
}

void MainPage::NewMessage(entities::Message message)
{
    chats_box_->UpdateChat(message.chat_id_, message.create_time_);
    chat_widgets_->AddMessageToChat(message);
}

void MainPage::DeleteMessage(int chat_id, int message_id)
{
    chat_widgets_->DeleteMessageToChat(chat_id, message_id);
}

void MainPage::UpdateMessageStatus(entities::Status status)
{
    chat_widgets_->UpdateStatusToMessage(status);
}

void MainPage::ChangeMessage(int chat_id, int message_id, QString text)
{
    chat_widgets_->ChangeMessage(chat_id, message_id, text);
}

void MainPage::ChangeMemberRole(entities::ChatMember member)
{
    auto chat_members = data::GeneralData::GetInstance()->getMembersToChat(member.chat_id_);
    if(!chat_members)
        return;

    chat_members->users_[member.user_id_].role_ = member.role_;

    if(chat_details_.find(member.chat_id_) == chat_details_.end()
        || chat_details_[member.chat_id_] == nullptr)
        return;

    chat_details_[member.chat_id_]->changeMemberRole(member.user_id_, member.role_);
}

void MainPage::DeleteChatMember(int chat_id, int member_id)
{
    if(member_id == data::GeneralData::GetInstance()->GetUserId())
    {
        chat_widgets_->DeleteChat(chat_id);
        chats_box_->DeleteChat(chat_id);

        if(current_chat_ == chat_id){
            chat_widgets_->hide();
            without_chat_->show();
        }
    }
    else
    {
        auto chat_members = data::GeneralData::GetInstance()->getMembersToChat(chat_id);
        if(!chat_members)
            return;

        chat_members->users_.erase(member_id);

        if(chat_details_.find(chat_id) == chat_details_.end()
            || chat_details_[chat_id] == nullptr)
            return;

        chat_details_[chat_id]->removeMember(member_id);
    }
}

QWidget* MainPage::CreateWidgetWithoutChat()
{
    QWidget* widget = new QWidget;
    widget->setObjectName("emptyChatWidget");
    widget->setStyleSheet(
        "QWidget#emptyChatWidget {"
        "    background-color: #0b0c0e;"
        "}"
        "QLabel {"
        "    color: #a1a1aa;"
        "    font-size: 14px;"
        "}");

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* label = new QLabel("выберите чат для общения");
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);

    return widget;
}

void MainPage::ResetState()
{
    current_chat_ = -1;

    for(auto& [id, widget] : chat_details_)
    {
        if(widget)
        {
            widget->deleteLater();
            widget = nullptr;
        }
    }
    chat_details_.clear();

    if(chat_widgets_)
    {
        chat_widgets_->deleteLater();
        chat_widgets_ = nullptr;
    }

    if(chats_box_)
    {
        chats_box_->deleteLater();
        chats_box_ = nullptr;
    }

    if(without_chat_)
    {
        without_chat_->deleteLater();
        without_chat_ = nullptr;
    }

    is_fill_ = false;
}


MainPage::~MainPage()
{
    delete ui;
}
