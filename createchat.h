#ifndef CREATECHAT_H
#define CREATECHAT_H

#include <QWidget>
#include <QTabBar>
#include <QTabWidget>

namespace Ui {

class CreateChat;
}

class CreateChat : public QWidget
{
    Q_OBJECT

public:
    explicit CreateChat(QWidget *parent = nullptr);
    ~CreateChat();

public slots:
    void ClickCreateDialog();
    void ClickCreateGroupChat();

private:
    Ui::CreateChat *ui;
};

#endif // CREATECHAT_H
