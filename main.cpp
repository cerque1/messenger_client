#include "log_reg_pages.h"
#include "mainpage.h"
#include "client.h"
#include "generaldata.h"
#include "messagehandler.h"

#include <QApplication>
#include <QPushButton>
#include <QMessageBox>
#include <QLineEdit>
#include <QErrorMessage>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qDebug() << QThread::currentThread();
    QThread* processor_thread = new QThread();
    auto messages_processor = std::make_shared<MessagesProcessor>();
    auto client = std::make_shared<Client>("ws://192.168.0.156:1234", messages_processor);
    messages_processor->moveToThread(processor_thread);
    client->moveToThread(processor_thread);
    processor_thread->start();

    data::GeneralData::GetInstance()->SetClient(client);
    MessageHandler* handler = new MessageHandler(&*data::GeneralData::GetInstance()->GetClient());
    MainPage page(handler, "ws://192.168.0.156:1234");
    std::unique_ptr<MainWindow> log_reg_window;

    QThread* thread = new QThread();
    data::GeneralData::GetInstance()->GetClient()->moveToThread(thread);
    thread->start();

    QMetaObject::invokeMethod(&*data::GeneralData::GetInstance()->GetClient(), "InitWebSocketSlot", Qt::QueuedConnection);

    QObject::connect(thread, SIGNAL(finished()), handler, SLOT(deleteLater()));
    QObject::connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    if(data::GeneralData::GetInstance()->GetToken().isEmpty()){
        log_reg_window = std::make_unique<MainWindow>(&page);
        page.setLogRegPage(log_reg_window.get());
        log_reg_window->show();
    }
    else {
        page.show();
    }

    return a.exec();
}
