#include "utils.h"

namespace utils{

void MakeMessageBox(QString text){
    QMessageBox msg_box;
    msg_box.setStyleSheet("background-color:rgb(71, 71, 74); color:rgb(235, 162, 255)");
    msg_box.setText(text);
    msg_box.exec();
}

}
