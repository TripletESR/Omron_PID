#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort>
#include <QModbusClient>
#include <QModbusDataUnit>
#include <QModbusRtuSerialMaster>
#include <QScrollBar>
#include <qcustomplot.h>

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
    QString formatHex(int value, int digit);
    void waitForMSec(int msec);
    void panalOnOff(bool IO);

    void read(QModbusDataUnit::RegisterType type, quint16 adress, int size, int respondFlag);
    void readReady();
    void write(int address, int value);
    void request(QModbusPdu::FunctionCode code, QByteArray cmd); // no PDU reply

    void askTemperature(int waitTime = 500);
    void askSetPoint(int waitTime = 500);
    void askRunStop(int waitTime = 500);
    void askAT(int waitTime = 500);
    void setAT(int atFlag);

    void on_pushButton_AskTemp_clicked();
    void on_lineEdit_Cmd_returnPressed();
    void on_pushButton_SetSV_clicked();
    void on_pushButton_Control_clicked();

    void on_comboBox_AT_currentIndexChanged(int index);
    void on_checkBox_RunSop_clicked();
    void on_pushButton_RecordTemp_clicked();
    void on_pushButton_ReadRH_clicked();
    void on_spinBox_DeviceAddress_valueChanged(int arg1);

    void on_pushButton_OpenFile_clicked();
    void on_pushButton_Connect_clicked();

private:
    Ui::MainWindow *ui;

    QCustomPlot * plot;

    QModbusRtuSerialMaster * omron;

    QString omronPortName;
    int omronID;

    int msgCount;
    int respondType;

    double temperature, SV;
    double tempDecimal;

    bool tempControlOnOff;
    bool tempRecordOnOff;
    bool modbusReady;
    bool atComboxEnable;

    QVector<QCPGraphData> timeData;
    double mean;

};

#endif // MAINWINDOW_H
