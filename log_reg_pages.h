#ifndef LOG_REG_PAGES_H
#define LOG_REG_PAGES_H

#include <QMainWindow>
#include "client.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* main_window, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    void OpenMainPage();

    Ui::MainWindow *ui;
    QWidget* main_window_;
};
#endif // LOG_REG_PAGES_H
