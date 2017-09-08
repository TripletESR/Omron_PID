#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort>
#include <QModbusClient>
#include <QModbusDataUnit>
#include <QModbusRtuSerialMaster>
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

    void showDataUnit(QModbusDataUnit unit);

private slots:
    // copy from web
    quint16 crc16ForModbus(const QByteArray &data);

    void on_pushButton_Ask_clicked();

    void read(QModbusDataUnit::RegisterType type, quint16 adress, int size, int respondFlag);
    void readReady();
    void write();
    void request(QByteArray cmd);

    void askTemperature();


    void on_lineEdit_Cmd_returnPressed();

private:
    Ui::MainWindow *ui;

    QModbusRtuSerialMaster * omron;

    QString omronPortName;
    int omronID;

    int msgCount;
    int respondType;
};

#endif // MAINWINDOW_H
