#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort>
//#include <QDebug>
//#include <QObject>
#include <QScrollBar>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void LogMsg(QString str);
    void findSeriesPortDevices();
    void write2SerialPort(const QString &msg);
    QString readFromSerialPort();

private:
    Ui::MainWindow *ui;

    QSerialPort * omron;

    QString omronPortName;

    int msgCount;
};

#endif // MAINWINDOW_H
