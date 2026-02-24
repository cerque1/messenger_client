#include "log_reg_pages.h"
#include "ui_log_reg_pages.h"
#include "request.h"
#include "response.h"
#include "utils.h"
#include "req_utils.h"
#include "generaldata.h"
#include "req_resp_utils.h"

#include <QMessageBox>
#include <QStackedWidget>
#include <QtNetwork/QTcpSocket>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>

MainWindow::MainWindow(QWidget* main_window, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), main_window_(main_window)
{
    ui->setupUi(this);
    // настройка страницы авторизации
    QObject::connect(ui->login_log, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
    QObject::connect(ui->register_log, &QPushButton::clicked, this, [this](){
        ui->stackedWidget->setCurrentIndex(1);
    });

    //натройка страницы регистрации
    QObject::connect(ui->register_reg, &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
    QObject::connect(ui->login_reg, &QPushButton::clicked, this, [this](){
        ui->stackedWidget->setCurrentIndex(0);
    });
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::onLoginClicked(){
    QLineEdit* login_edit = ui->login_edit_log;
    QLineEdit* password_edit = ui->password_log;
    if(login_edit->text().isEmpty() || password_edit->text().isEmpty()){
        utils::MakeMessageBox("заполнены не все поля!");
        return;
    }
    Request req = req_utils::MakeLoginRequest(login_edit->text(),
                                              password_edit->text());
    qDebug() << req.getJsonRequest();
    Response resp;
    try {
        resp = req_resp_utils::SendReqAndWaitResp(req);
    } catch(std::runtime_error& e){
        qDebug() << e.what();
        utils::MakeMessageBox("сервер не отвечает");
    }

    int status = resp.getStatus();
    if(status != 200){
        if(status == 401){
            utils::MakeMessageBox("введены неверные данные");
        }
        else {
            utils::MakeMessageBox("ошибка авторизации");
        }
        return;
    }
    qDebug() << resp.getValueFromBody("token").toString();
    data::GeneralData* data_ = data::GeneralData::GetInstance();
    data_->SetToken(resp.getValueFromBody("token").toString());
    data_->SetUserId(resp.getValueFromBody("user_id").toInt());
    data_->SetUserName(resp.getValueFromBody("user_name").toString());
    OpenMainPage();
}

void MainWindow::onRegisterClicked(){
    QLineEdit* login_edit = ui->login_edit_reg;
    QLineEdit* password_edit = ui->password_reg;
    QLineEdit* nickname_edit = ui->nickname;
    if(login_edit->text().isEmpty() || password_edit->text().isEmpty()
        || nickname_edit->text().isEmpty() || ui->birthday_edit->text().isEmpty()){
        utils::MakeMessageBox("заполнены не все поля!");
        return;
    }

    Request req = req_utils::MakeRegistrationRequest(login_edit->text(),
                                                     password_edit->text(),
                                                     nickname_edit->text(),
                                                     ui->birthday_edit->dateTime().toString("yyyy-MM-dd hh:mm:ss"),
                                                     QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                                                     QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    qDebug() << req.getJsonRequest();
    Response resp;
    try {
        resp = req_resp_utils::SendReqAndWaitResp(req);
    } catch(std::runtime_error& e){
        qDebug() << e.what();
        utils::MakeMessageBox("сервер не отвечает");
    }

    int status = resp.getStatus();
    if(status != 200){
        utils::MakeMessageBox("ошибка регистрации");
        return;
    }
    data::GeneralData::GetInstance()->SetToken(resp.getValueFromBody("token").toString());
    data::GeneralData::GetInstance()->SetUserId(resp.getValueFromBody("user_id").toInt());
    data::GeneralData::GetInstance()->SetUserName(resp.getValueFromBody("user_name").toString());
    OpenMainPage();
}

void MainWindow::OpenMainPage(){
    ui->login_edit_log->setText("");
    ui->password_log->setText("");
    ui->login_edit_reg->setText("");
    ui->password_reg->setText("");
    ui->nickname->setText("");

    main_window_->show();
    this->close();
}


