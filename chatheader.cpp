#include "chatheader.h"
#include "ui_chatheader.h"

#include <QPushButton>

ChatHeader::ChatHeader(int chat_id, QString chat_name, QString last_time, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatHeader)
    , chat_id_(chat_id)
    , chat_name_(chat_name)
{
    ui->setupUi(this);
    ui->name->setText(chat_name);
    ui->time->setText(last_time);

    connect(ui->start_call_button, &QPushButton::clicked, this, &ChatHeader::startCallSlot);
}

void ChatHeader::SetChatName(QString name){
    ui->name->setText(name);
}

void ChatHeader::SetChatTime(QString time){
    ui->time->setText(time);
}

void ChatHeader::startCallSlot() {
    emit startCallClicked(chat_id_, chat_name_);
}

ChatHeader::~ChatHeader()
{
    delete ui;
}
