#include "chatdetails.h"
#include "ui_chatdetails.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QMenu>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include <algorithm>

#include "request.h"
#include "generaldata.h"
#include "req_resp_utils.h"

ChatDetails::ChatDetails(int chat_id, QString chat_name, bool isDialog, QWidget *parent)
    : QWidget(parent),
    ui(new Ui::ChatDetails),
    chat_id_(chat_id),
    chat_name_(chat_name),
    is_dialog_(isDialog)
{
    ui->setupUi(this);

    current_user_id_ = data::GeneralData::GetInstance()->GetUserId();

    ui->chat_name->setText(chat_name_);
    connect(ui->back, &QPushButton::clicked, this, &ChatDetails::BackClick);

    media_list_ = new QListWidget;
    media_list_->setStyleSheet(
        "QListWidget {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    color: #ffffff;"
        "}"
        "QListWidget::item {"
        "    background-color: #15171a;"
        "    border-radius: 8px;"
        "    padding: 4px;"
        "    margin: 2px;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #181b1f;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #2563eb;"
        "}");
    files_list_ = new QListWidget;
    files_list_->setStyleSheet(
        "QListWidget {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    color: #ffffff;"
        "}"
        "QListWidget::item {"
        "    background-color: #15171a;"
        "    border-radius: 8px;"
        "    padding: 4px;"
        "    margin: 2px;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #181b1f;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #2563eb;"
        "}");

    ui->tabWidget->widget(0)->setLayout(new QVBoxLayout);
    ui->tabWidget->widget(0)->layout()->addWidget(media_list_);

    ui->tabWidget->widget(1)->setLayout(new QVBoxLayout);
    ui->tabWidget->widget(1)->layout()->addWidget(files_list_);

    if (!is_dialog_)
    {
        QWidget *members = new QWidget;
        QVBoxLayout *lay = new QVBoxLayout(members);

        add_member_btn_ = new QPushButton("Добавить участника");
        add_member_btn_->setStyleSheet(
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
        members_list_ = new QListWidget;
        members_list_->setStyleSheet(
            "QListWidget {"
            "    background-color: #15171a;"
            "    border: 1px solid #23262b;"
            "    border-radius: 10px;"
            "    color: #ffffff;"
            "}"
            "QListWidget::item {"
            "    background-color: #15171a;"
            "    border-radius: 8px;"
            "    padding: 8px;"
            "    margin: 2px;"
            "}"
            "QListWidget::item:hover {"
            "    background-color: #181b1f;"
            "}"
            "QListWidget::item:selected {"
            "    background-color: #2563eb;"
            "}");

        lay->addWidget(add_member_btn_);
        lay->addWidget(members_list_);

        ui->tabWidget->insertTab(0, members, "Участники");

        connect(add_member_btn_, &QPushButton::clicked,
                this, &ChatDetails::showAddMemberDialog);

        members_list_->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(members_list_, &QListWidget::customContextMenuRequested,
                this, [=](const QPoint &pos)
                {
                    QListWidgetItem *it = members_list_->itemAt(pos);
                    if (!it) return;

                    int targetId = it->data(Qt::UserRole).toInt();
                    int targetRole = it->data(Qt::UserRole + 1).toInt();

                    if (targetId == current_user_id_)
                        return;

                    int myRole = 2;
                    for (int i = 0; i < members_list_->count(); ++i)
                    {
                        auto *item = members_list_->item(i);
                        if (item->data(Qt::UserRole).toInt() == current_user_id_)
                        {
                            myRole = item->data(Qt::UserRole + 1).toInt();
                            break;
                        }
                    }

                    QMenu menu;
                    menu.setStyleSheet(
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

                    QAction *changeRole = nullptr;
                    if (myRole == 0)
                        changeRole = menu.addAction("Изменить роль");

                    QAction *removeAct = nullptr;
                    if (myRole < targetRole)
                        removeAct = menu.addAction("Удалить из чата");

                    QAction *res = menu.exec(members_list_->mapToGlobal(pos));
                    if (!res) return;

                    else if (res == changeRole)
                    {
                        QDialog dlg(this);
                        dlg.setWindowTitle("Выдать роль");
                        dlg.setStyleSheet(
                            "QDialog {"
                            "    background-color: #0b0c0e;"
                            "    color: #e6e6e6;"
                            "}");

                        QVBoxLayout lay(&dlg);
                        lay.setContentsMargins(16, 16, 16, 16);
                        lay.setSpacing(12);
                        QComboBox box;
                        box.setStyleSheet(
                            "QComboBox {"
                            "    background-color: #15171a;"
                            "    border: 1px solid #23262b;"
                            "    border-radius: 10px;"
                            "    padding: 5px 12px;"
                            "    color: #ffffff;"
                            "}"
                            "QComboBox:hover {"
                            "    background-color: #181b1f;"
                            "    border: 1px solid #2f3339;"
                            "}"
                            "QComboBox::drop-down {"
                            "    border: none;"
                            "}"
                            "QComboBox QAbstractItemView {"
                            "    background-color: #15171a;"
                            "    border: 1px solid #23262b;"
                            "    border-radius: 10px;"
                            "    color: #ffffff;"
                            "    selection-background-color: #2563eb;"
                            "}");

                        for (int r = myRole + 1; r <= 2; r++)
                            if (r != targetRole)
                                box.addItem(roleToString(r), r);

                        QPushButton ok("Выдать");
                        ok.setStyleSheet(
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
                            "}");

                        lay.addWidget(&box);
                        lay.addWidget(&ok);

                        connect(&ok, &QPushButton::clicked, &dlg, &QDialog::accept);

                        if (dlg.exec() == QDialog::Accepted)
                        {
                            int newRole = box.currentData().toInt();

                            Request req;
                            req.setPath("change_member_role");
                            req.setValueToBody("chat_id", chat_id_);
                            req.setValueToBody("user_id", targetId);
                            req.setValueToBody("role", newRole);
                            req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());

                            Response resp = req_resp_utils::SendReqAndWaitResp(req);

                            if (resp.getStatus() != 200)
                            {
                                QMessageBox::warning(this, "Ошибка",
                                                     resp.getValueFromBody("message").toString());
                                return;
                            }
                        }
                    }
                    else if (res == removeAct)
                    {
                        Request req;
                        req.setPath("delete_member_from_chat");
                        req.setValueToBody("chat_id", chat_id_);
                        req.setValueToBody("user_id", targetId);
                        req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());

                        Response resp = req_resp_utils::SendReqAndWaitResp(req);

                        if (resp.getStatus() != 200)
                        {
                            QMessageBox::warning(this, "Ошибка",
                                                 resp.getValueFromBody("message").toString());
                            return;
                        }
                    }
                });

        auto chat_members = data::GeneralData::GetInstance()->getMembersToChat(chat_id);
        for (auto [user_id, user_info] : chat_members->users_)
            addMemberToList(user_info.name_, user_id, user_info.role_);
    }

    sendRequest();
}

ChatDetails::~ChatDetails()
{
    delete ui;
}

bool ChatDetails::isImage(const QString &n)
{
    QString s = n.toLower();
    return s.endsWith(".png") ||
           s.endsWith(".jpg") ||
           s.endsWith(".jpeg") ||
           s.endsWith(".webp") ||
           s.endsWith(".bmp") ||
           s.endsWith(".gif");
}

QString ChatDetails::roleToString(int role) const
{
    if (role == 0) return "владелец";
    if (role == 1) return "администратор";
    return "участник";
}

void ChatDetails::addMemberToList(const QString &username, int userid, int role)
{
    QListWidgetItem *item = new QListWidgetItem;
    item->setText(username + " | " + roleToString(role));
    item->setData(Qt::UserRole, userid);
    item->setData(Qt::UserRole + 1, role);

    members_list_->addItem(item);
    sortMembers();
}

void ChatDetails::sortMembers()
{
    QList<QListWidgetItem *> items;

    while (members_list_->count())
        items.append(members_list_->takeItem(0));

    std::sort(items.begin(), items.end(),
              [&](QListWidgetItem *a, QListWidgetItem *b)
              {
                  int idA = a->data(Qt::UserRole).toInt();
                  int idB = b->data(Qt::UserRole).toInt();

                  if (idA == current_user_id_) return true;
                  if (idB == current_user_id_) return false;

                  int rA = a->data(Qt::UserRole + 1).toInt();
                  int rB = b->data(Qt::UserRole + 1).toInt();

                  return rA < rB;
              });

    for (auto *i : items)
        members_list_->addItem(i);
}

void ChatDetails::changeMemberRole(int userId, int newRole)
{
    for (int i = 0; i < members_list_->count(); ++i)
    {
        auto *it = members_list_->item(i);
        if (it->data(Qt::UserRole).toInt() == userId)
        {
            QString name = it->text().split("|").first().trimmed();
            it->setText(name + " | " + roleToString(newRole));
            it->setData(Qt::UserRole + 1, newRole);
            break;
        }
    }
    sortMembers();
}

void ChatDetails::removeMember(int userId)
{
    for (int i = 0; i < members_list_->count(); ++i)
    {
        auto *it = members_list_->item(i);
        if (it->data(Qt::UserRole).toInt() == userId)
        {
            delete members_list_->takeItem(i);
            return;
        }
    }
}

void ChatDetails::showAddMemberDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Добавить участника");
    dlg.setStyleSheet(
        "QDialog {"
        "    background-color: #0b0c0e;"
        "    color: #e6e6e6;"
        "}");

    QVBoxLayout lay(&dlg);
    lay.setContentsMargins(16, 16, 16, 16);
    lay.setSpacing(12);

    QLineEdit edit;
    edit.setPlaceholderText("Введите username");
    edit.setStyleSheet(
        "QLineEdit {"
        "    background-color: #15171a;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    padding-left: 12px;"
        "    height: 32px;"
        "    color: #ffffff;"
        "    selection-background-color: #3b82f6;"
        "}"
        "QLineEdit:hover {"
        "    background-color: #181b1f;"
        "    border: 1px solid #2f3339;"
        "}"
        "QLineEdit:focus {"
        "    background-color: #1b1e23;"
        "    border: 1px solid #3b82f6;"
        "}");

    QPushButton ok("Добавить");
    ok.setStyleSheet(
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
        "}");

    lay.addWidget(&edit);
    lay.addWidget(&ok);

    connect(&ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted)
    {
        QString username = edit.text();
        if (username.isEmpty()) return;

        Request req;
        req.setPath("add_member_to_chat");
        req.setValueToBody("chat_id", chat_id_);
        req.setValueToBody("user_name", username);
        req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());

        Response resp = req_resp_utils::SendReqAndWaitResp(req);

        if (resp.getStatus() != 200)
            QMessageBox::warning(this, "Ошибка",
                                 resp.getValueFromBody("message").toString());
    }
}

void ChatDetails::sendRequest()
{
    Request req;
    req.setPath("get_content_to_chat");
    req.setValueToBody("chat_id", chat_id_);
    req.setValueToBody("token", data::GeneralData::GetInstance()->GetToken());

    Response resp = req_resp_utils::SendReqAndWaitResp(req);
    auto contents = req_resp_utils::MakeContentsFromResponse(resp);

    for (auto content : contents)
    {
        bool media = isImage(content.filename_);

        addItem(media ? media_list_ : files_list_,
                content.message_id_,
                content.filename_,
                media);
    }
}

void ChatDetails::addItem(QListWidget *list, int message_id, const QString &filename, bool isMedia)
{
    QListWidgetItem *item = new QListWidgetItem(list);
    item->setSizeHint(QSize(0, isMedia ? 100 : 50));

    QWidget *w = new QWidget;
    w->setStyleSheet("QWidget { background-color: #15171a; border-radius: 8px; }");
    QHBoxLayout *l = new QHBoxLayout(w);
    l->setContentsMargins(8, 8, 8, 8);

    if (isMedia)
    {
        QLabel *img = new QLabel;
        img->setFixedSize(100, 100);
        img->setStyleSheet("background-color: #181b1f; border-radius: 8px;");

        QString path = "temp/files/" + filename;
        QPixmap pix(path);

        if (!pix.isNull())
            img->setPixmap(pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            img->setStyleSheet("background-color: #181b1f; border-radius: 8px;");

        l->addWidget(img);
    }

    QLabel *name = new QLabel(filename);
    name->setStyleSheet("color: #ffffff; font-size: 12px;");
    l->addWidget(name);
    l->addStretch();

    list->setItemWidget(item, w);

    connect(list, &QListWidget::itemClicked, this,
            [=](QListWidgetItem *it)
            {
                if (it == item)
                    emit ContentClicked(message_id);
            });
}
