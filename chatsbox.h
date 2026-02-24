#ifndef CHATSBOX_H
#define CHATSBOX_H

#include <QVBoxLayout>
#include <QWidget>
#include <QObject>
#include <QPushButton>

#include "chat.h"
#include "entities.h"

class ChatsBox : public QVBoxLayout
{
    Q_OBJECT
public:

    explicit ChatsBox(QWidget* parent = nullptr);

    void AddOrUpdateChat(const entities::Chat& chat);
    void UpdateChat(int chat_id, QString last_update_time);
    void DeleteChat(int chat_id);

    entities::Chat getChatById(int chat_id){
        return chats_[chat_id];
    }

signals:
    void Click(Chat*);
    void DeleteChatForMe(int,int);

private slots:
    void ClickToButton(){
        emit Click(qobject_cast<Chat*>(sender()));
    }
    void updateActionSlot();
    void deleteActionSlot();

private:
    Chat* FormChatWidget(const entities::Chat& chat);

    std::unordered_map<int, entities::Chat> chats_;
    std::unordered_map<int, Chat*> chat_widgets_;
};

#endif // CHATSBOX_H
