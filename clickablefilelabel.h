#ifndef CLICKABLEFILELABEL_H
#define CLICKABLEFILELABEL_H

#include <QLabel>
#include <QMouseEvent>

class ClickableFileLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableFileLabel(QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
};

#endif // CLICKABLEFILELABEL_H
