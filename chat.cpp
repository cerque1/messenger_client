#include "chat.h"
#include "ui_chat.h"
#include "generaldata.h"

#include <QMenu>

Chat::Chat(const entities::Chat& chat, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::chat)
{
    ui->setupUi(this);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenuSlot(QPoint)));

    QStringList parts = chat.name_.split('#');
    QString another_user = (parts[0] == data::GeneralData::GetInstance()->GetUserName()) ? parts[1] : parts[0];

    ui->chat_name->setText(another_user);

    QDateTime dt = QDateTime::fromString(
        chat.last_update_time_,
        Qt::ISODate
        );
    dt.setTimeZone(QTimeZone::UTC);
    dt = dt.toLocalTime();
    QString result;


    if (dt.isValid())
    {
        QDateTime now = QDateTime::currentDateTime();
        QDate today = now.date();
        QDate msgDate = dt.date();

        if (msgDate == today)
        {
            result = dt.toString("HH:mm");
        }
        else
        {
            int daysFromMonday = today.dayOfWeek() - 1;
            QDate startOfWeek = today.addDays(-daysFromMonday);

            if (msgDate >= startOfWeek)
            {
                QString weekday =
                    QLocale().standaloneDayName(
                        msgDate.dayOfWeek(),
                        QLocale::ShortFormat
                        );

                result = QString("%1 %2")
                             .arg(weekday)
                             .arg(dt.toString("HH:mm"));
            }
            else if (msgDate.year() == today.year())
            {
                result = dt.toString("dd.MM HH:mm");
            }
            else
            {
                result = dt.toString("dd.MM.yy");
            }
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
    QAction* update_action = new QAction(QString::fromUtf8("Изменить"), this);
    connect(update_action, SIGNAL(triggered(bool)), this, SIGNAL(updateAction()));
    QAction* delete_action = new QAction(QString::fromUtf8("Удалить"), this);
    connect(delete_action, SIGNAL(triggered(bool)), this, SIGNAL(deleteAction()));
    message_menu->addAction(update_action);
    message_menu->addSeparator();
    message_menu->addAction(delete_action);
    message_menu->popup(point_pos);
}
