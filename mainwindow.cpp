#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "request.h"
#include "response.h"

#include <QMessageBox>
#include <QStackedWidget>
#include <QtNetwork/QTcpSocket>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>


//
// Заменить поиск элементов на обращение ui->(id элемента)
//
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), client(new Client("127.0.0.1", this))
{
    ui->setupUi(this);
    QStackedWidget* stack_widget = qobject_cast<QStackedWidget*>(this->findChild<QObject*>("stackedWidget"));
    // настройка страницы авторизации
    auto* login_page = stack_widget->widget(0);
    QPushButton* login_button = qobject_cast<QPushButton*>(login_page->findChild<QObject*>("login_log"));
    QPushButton* go_to_register_button = qobject_cast<QPushButton*>(login_page->findChild<QObject*>("register_log"));
    QObject::connect(login_button, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
    QObject::connect(go_to_register_button, &QPushButton::clicked, this, [stack_widget](){
        stack_widget->setCurrentIndex(1);
    });

    //натройка страницы регистрации
    auto* register_page = stack_widget->widget(1);
    QPushButton* go_to_login_button = qobject_cast<QPushButton*>(register_page->findChild<QObject*>("login_reg"));
    QPushButton* register_button = qobject_cast<QPushButton*>(register_page->findChild<QObject*>("register_reg"));
    QObject::connect(register_button, &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
    QObject::connect(go_to_login_button, &QPushButton::clicked, this, [stack_widget](){
        stack_widget->setCurrentIndex(0);
    });
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::onLoginClicked(){
    QLineEdit* email_edit = qobject_cast<QLineEdit*>(this->findChild<QObject*>("email_log"));
    QLineEdit* password_edit = qobject_cast<QLineEdit*>(this->findChild<QObject*>("password_log"));
    if(email_edit->text().isEmpty() || password_edit->text().isEmpty()){
        QMessageBox msg_box;
        msg_box.setStyleSheet("background-color:rgb(71, 71, 74); color:rgb(235, 162, 255)");
        msg_box.setText("заполнены не все поля!");
        msg_box.exec();
        return;
    }
    Request req;
    req.setPath("login");
    req.setValueToBody("login", email_edit->text());
    req.setValueToBody("password", password_edit->text());
    qDebug() << req.getJsonRequest();
    Response resp = Response::fromJson(client->SendRequest(req.getJsonRequest()));

    QMessageBox msg_box;
    msg_box.setStyleSheet("background-color:rgb(71, 71, 74); color:rgb(235, 162, 255)");
    msg_box.setText(resp.getJsonResponse());
    msg_box.exec();
}

void MainWindow::onRegisterClicked(){
    QLineEdit* email_edit = qobject_cast<QLineEdit*>(this->findChild<QObject*>("email_reg"));
    QLineEdit* password_edit = qobject_cast<QLineEdit*>(this->findChild<QObject*>("password_reg"));
    QLineEdit* nickname_edit = qobject_cast<QLineEdit*>(this->findChild<QObject*>("nickname"));
    if(email_edit->text().isEmpty() || password_edit->text().isEmpty() || nickname_edit->text().isEmpty()){
        QMessageBox msg_box;
        msg_box.setStyleSheet("background-color:rgb(71, 71, 74); color:rgb(235, 162, 255)");
        msg_box.setText("заполнены не все поля!");
        msg_box.exec();
        return;
    }
    Request req;
    req.setPath("registrate");
    req.setValueToBody("login", email_edit->text());
    req.setValueToBody("password", password_edit->text());
    req.setValueToBody("name", nickname_edit->text());
    req.setValueToBody("birthday", "2006-06-09");
    req.setValueToBody("registration_day", "2006-06-09");
    req.setValueToBody("last_online", "2006-06-09");
    qDebug() << req.getJsonRequest();
    Response resp = Response::fromJson(client->SendRequest(req.getJsonRequest()));

    QMessageBox msg_box;
    msg_box.setStyleSheet("background-color:rgb(71, 71, 74); color:rgb(235, 162, 255)");
    msg_box.setText(resp.getJsonResponse());
    msg_box.exec();
}


