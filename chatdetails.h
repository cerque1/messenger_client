#ifndef CHATDETAILS_H
#define CHATDETAILS_H

#include <QWidget>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QPushButton>

namespace Ui {
class ChatDetails;
}

class ChatDetails : public QWidget
{
    Q_OBJECT

public:
    explicit ChatDetails(int chat_id,
                         QString chat_name,
                         bool isDialog,
                         QWidget *parent = nullptr);
    ~ChatDetails();

    void addMemberToList(const QString& username,int userid,int role);

    void changeMemberRole(int userId,int newRole);
    void removeMember(int userId);

signals:
    void BackClick();
    void ContentClicked(int message_id);

private:
    bool isImage(const QString& name);
    void sendRequest();
    void addItem(QListWidget* list,int message_id,const QString& filename,bool isMedia);

    QString roleToString(int role) const;
    void sortMembers();
    void showAddMemberDialog();

private:
    Ui::ChatDetails *ui;

    int chat_id_;
    QString chat_name_;
    bool is_dialog_;

    int current_user_id_ = -1;

    QListWidget* media_list_;
    QListWidget* files_list_;
    QListWidget* members_list_{nullptr};
    QPushButton* add_member_btn_{nullptr};
};

#endif
