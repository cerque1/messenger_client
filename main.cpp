#include "log_reg_pages.h"
#include "mainpage.h"
#include "client.h"
#include "generaldata.h"
#include "messagehandler.h"
#include "tokenstore.h"

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
    auto client = std::make_shared<Client>("ws://localhost:1234", messages_processor);
    messages_processor->moveToThread(processor_thread);
    client->moveToThread(processor_thread);
    processor_thread->start();

    data::GeneralData::GetInstance()->SetClient(client);
    const token_store::SessionData saved_session = token_store::LoadSession();
    data::GeneralData::GetInstance()->SetToken(saved_session.token);
    data::GeneralData::GetInstance()->SetUserId(saved_session.user_id);
    data::GeneralData::GetInstance()->SetUserName(saved_session.user_name);
    MessageHandler* handler = new MessageHandler(&*data::GeneralData::GetInstance()->GetClient());
    MainPage page(handler, "ws://localhost:1234");
    std::unique_ptr<MainWindow> log_reg_window;

    QThread* thread = new QThread();
    data::GeneralData::GetInstance()->GetClient()->moveToThread(thread);
    thread->start();

    QMetaObject::invokeMethod(&*data::GeneralData::GetInstance()->GetClient(), "InitWebSocketSlot", Qt::QueuedConnection);

    QObject::connect(thread, SIGNAL(finished()), handler, SLOT(deleteLater()));
    QObject::connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        token_store::SessionData session;
        session.token = data::GeneralData::GetInstance()->GetToken();
        session.user_id = data::GeneralData::GetInstance()->GetUserId();
        session.user_name = data::GeneralData::GetInstance()->GetUserName();

        if (session.token.isEmpty()) {
            token_store::RemoveToken();
        } else {
            token_store::SaveSession(session);
        }
    });

    log_reg_window = std::make_unique<MainWindow>(&page);
    page.setLogRegPage(log_reg_window.get());

    if(data::GeneralData::GetInstance()->GetToken().isEmpty()){
        log_reg_window->show();
    }
    else {
        page.show();
    }

    return a.exec();
}
