#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QGraphicsScene>

#include <QString>
#include <QStringList>

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

const QString PATH="C:/Users/Public/Pictures/Sample Pictures";

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private slots:
    void openFileDialog();
    void readJpegFile();
    void dumpIfdTableToPlainTextEdit(void *pIfd);
    char *getTagName(int ifdType, unsigned short tagId);

private:
    Ui::Widget *ui;
    QGraphicsScene scene;
};

#endif // WIDGET_H
