#include "createchat.h"
#include "ui_createchat.h"
#include "utils.h"
#include "response.h"
#include "req_resp_utils.h"
#include "generaldata.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

CreateChat::CreateChat(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CreateChat)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    connect(ui->gchat_button, SIGNAL(clicked()), this, SLOT(ClickCreateGroupChat()));
    connect(ui->nick_button, SIGNAL(clicked()), this, SLOT(ClickCreateDialog()));
}

void CreateChat::ClickCreateDialog(){
    if(ui->nick_edit->text().isEmpty()){
        utils::MakeMessageBox("не заполнено поле ввода никнейма");
        return;
    }

    Response resp = req_resp_utils::SendReqAndWaitResp(req_resp_utils::MakeCreateDialogRequest(data::GeneralData::GetInstance()->GetToken(),
                                                                                      ui->nick_edit->text()));

    if(resp.getStatus() != 200){
        utils::MakeMessageBox("Ошибка создания диалога");
        return;
    }
    close();
}

void CreateChat::ClickCreateGroupChat(){
    if(ui->gchat_edit->text().isEmpty()){
        utils::MakeMessageBox("не заполнено поле ввода никнейма");
        return;
    }

    Response resp = req_resp_utils::SendReqAndWaitResp(req_resp_utils::MakeCreateGroupChatRequest(data::GeneralData::GetInstance()->GetToken(),
                                                                                                  ui->gchat_edit->text()));

    if(resp.getStatus() != 200){
        utils::MakeMessageBox("Ошибка создания чата");
        return;
    }
    close();
}

CreateChat::~CreateChat()
{
    delete ui;
}
