#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QMessageBox>

namespace utils{

std::shared_ptr<QMessageBox> MakeMessageBox(QString text);

}


#endif // UTILS_H
