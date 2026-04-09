#include "utils.h"

namespace utils{

std::shared_ptr<QMessageBox> MakeMessageBox(QString text){
    std::shared_ptr<QMessageBox> msg_box = std::make_shared<QMessageBox>();
    msg_box->setStyleSheet("background-color:rgb(71, 71, 74); color:rgb(235, 162, 255)");
    msg_box->setText(text);
    msg_box->exec();
    return msg_box;
}

}
