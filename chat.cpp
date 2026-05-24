#include "chat.h"
#include "ui_chat.h"
#include "generaldata.h"

#include <QMenu>
#include <QLocale>

namespace {
QString FormatMessageTime(const QString& iso_time) {
    QDateTime dt = QDateTime::fromString(iso_time, Qt::ISODate);
    dt.setTimeSpec(Qt::UTC);
    dt = dt.toLocalTime();
    if(!dt.isValid()){
        return "";
    }
    return dt.toString("HH:mm");
}
}

Chat::Chat(const entities::Chat& chat, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::chat)
{
    ui->setupUi(this);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenuSlot(QPoint)));

    QStringList parts = chat.name_.split('#');
    QString another_user = (parts[0] == data::GeneralData::GetInstance()->GetUserName()) ? parts[1] : parts[0];

    if(chat.has_unread_messages_){
        ui->chat_name->setText(QString::fromUtf8("● %1").arg(another_user));
        ui->chat_name->setStyleSheet(
            "color: #60a5fa;"
            "font-weight: 700;"
            "font-size: 14px;");
        ui->last_mess->setStyleSheet(
            "color: #e2e8f0;"
            "font-size: 12px;"
            "font-weight: 600;");
        this->setStyleSheet(
            "QWidget#chat {"
            "    background-color: #1b2433;"
            "    border: 1px solid #3b82f6;"
            "    border-radius: 10px;"
            "    padding: 8px 12px;"
            "    margin: 4px 0px;"
            "}"
            "QWidget#chat:hover {"
            "    background-color: #212d40;"
            "    border: 1px solid #60a5fa;"
            "}");
    } else {
        ui->chat_name->setText(another_user);
    }

    QString result;
    if(chat.has_last_message_){
        const auto& last_message = chat.last_message_;
        result = last_message.text_;
        QString time_str = FormatMessageTime(last_message.create_time_);
        if(!time_str.isEmpty()){
            result = QString("%1 • %2").arg(result, time_str);
        }
    }

    ui->last_mess->setText(result);

    SetChatId(chat.id_);
}

Chat::~Chat()
{
    delete ui;
}

QString Chat::GetName() const{
    return ui->chat_name->text();
}

QString Chat::GetLast() const{
    return ui->last_mess->text();
}

void Chat::showContextMenuSlot(const QPoint &pos){
    QPoint point_pos = dynamic_cast<QWidget*>(sender())->mapToGlobal(pos);

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
    QAction* delete_action = new QAction(QString::fromUtf8("Удалить"), this);
    connect(delete_action, SIGNAL(triggered(bool)), this, SIGNAL(deleteAction()));
    message_menu->addAction(delete_action);
    message_menu->popup(point_pos);
}
