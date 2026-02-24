#include "chatsbox.h"
#include "generaldata.h"
#include "req_resp_utils.h"
#include "utils.h"

#include <QWidget>
#include <QPushButton>
#include <QMenu>

ChatsBox::ChatsBox(QWidget* parent)
    : QVBoxLayout(parent){
    setObjectName("chats");
    setAlignment(Qt::AlignTop);
    setSizeConstraint(QLayout::SetMinimumSize);
}

void ChatsBox::AddOrUpdateChat(const entities::Chat& chat){
    QWidget* chat_w = chat_widgets_[chat.id_];
    if(chat_w != nullptr){
        chat_w->hide();
        removeWidget(chat_w);
    }

    Chat* new_chat_w = FormChatWidget(chat);
    insertWidget(0, new_chat_w);
    chats_[chat.id_] = chat;
    chat_widgets_[chat.id_] = new_chat_w;
}

void ChatsBox::UpdateChat(int chat_id, QString last_update_time){
    if(chats_.find(chat_id) == chats_.end()){
        return;
    }

    chats_[chat_id].last_update_time_ = last_update_time;
    QWidget* chat_w = chat_widgets_[chat_id];
    if(chat_w != nullptr){
        chat_w->hide();
        removeWidget(chat_w);
    }
    Chat* new_chat_w = FormChatWidget(chats_[chat_id]);
    insertWidget(0, new_chat_w);
    chat_widgets_[chat_id] = new_chat_w;
}

void ChatsBox::updateActionSlot(){
    qDebug() << "update message click";
}

void ChatsBox::deleteActionSlot(){
    Chat* button = qobject_cast<Chat*>(sender());
    int chat_id = button->GetChatId();

    auto general_data = data::GeneralData::GetInstance();
    Request req = req_resp_utils::MakeDeleteChatMemberRequest(general_data->GetToken(),
                                                              chat_id,
                                                              general_data->GetUserId());

    Response resp = req_resp_utils::SendReqAndWaitResp(req);
    if(resp.getStatus() != 200) {
        utils::MakeMessageBox(resp.getValueFromBody("message").toString());
        return;
    }

    emit DeleteChatForMe(chat_id, general_data->GetUserId());
}

void ChatsBox::DeleteChat(int chat_id) {
    this->removeWidget(chat_widgets_.at(chat_id));
    chat_widgets_.at(chat_id)->hide();
    chats_.erase(chat_id);
    chat_widgets_.erase(chat_id);
    setSizeConstraint(QLayout::SetMinimumSize);
}

Chat* ChatsBox::FormChatWidget(const entities::Chat& chat){
    Chat* button = new Chat(chat);
    button->setParent(this->widget());
    button->setObjectName(QString::number(chat.id_));
    connect(button, SIGNAL(clicked()), this, SLOT(ClickToButton()));
    button->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(button, SIGNAL(updateAction()), this, SLOT(updateActionSlot()));
    connect(button, SIGNAL(deleteAction()), this, SLOT(deleteActionSlot()));

    return button;
}
