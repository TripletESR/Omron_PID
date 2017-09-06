#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    msgCount = 0;

    findSeriesPortDevices();

    omron = new QSerialPort(this);
    omron->setPortName(omronPortName);
    omron->setBaudRate(QSerialPort::Baud115200);
    omron->setDataBits(QSerialPort::Data8);
    omron->setParity(QSerialPort::NoParity);
    omron->setStopBits(QSerialPort::OneStop);
    omron->setFlowControl(QSerialPort::NoFlowControl);

    connect(omron, &QSerialPort::readyRead, this, &MainWindow::readFromSerialPort);

    if(omron->open(QIODevice::ReadWrite)){
        LogMsg("The Omron temperature control is connected in " + omronPortName + ".");
    }else{
        LogMsg("The Omron temperature control cannot be found on any COM port.");
    }
}

MainWindow::~MainWindow()
{
    delete omron;
    delete ui;
}

void MainWindow::LogMsg(QString str)
{
    msgCount ++;
    QString dateStr = QDateTime::currentDateTime().toString("HH:mm:ss ");
    QString countStr;
    countStr.sprintf("[%04d]: ", msgCount);
    str.insert(0, countStr).insert(0, dateStr);
    ui->textEdit_Log->append(str);
    int max = ui->textEdit_Log->verticalScrollBar()->maximum();
    ui->textEdit_Log->verticalScrollBar()->setValue(max);
}

void MainWindow::findSeriesPortDevices()
{
    //static const char blankString[] = QT_TRANSLATE_NOOP("SettingsDialog", "N/A");
    LogMsg("--------------");
    LogMsg("found COM Ports:");
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        //LogMsg("PortName     ="+info.portName()                                                                         );
        //LogMsg("description  ="+(!info.description().isEmpty() ?  info.description() : blankString)                                    );
        //LogMsg("manufacturer ="+(!info.manufacturer().isEmpty() ? info.manufacturer() : blankString)                                  );
        //LogMsg("serialNumber ="+(!info.serialNumber().isEmpty() ? info.serialNumber() : blankString)                                  );
        //LogMsg("Location     ="+info.systemLocation()                                                                   );
        //LogMsg("Vendor       ="+(info.vendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : blankString)  );
        //LogMsg("Identifier   ="+(info.productIdentifier() ? QString::number(info.productIdentifier(), 16) : blankString));
        //LogMsg("=======================");

        LogMsg(info.portName() + ", " + info.serialNumber() + ", " + info.manufacturer());

        if(info.serialNumber() == "FT0875RPA" && info.manufacturer() == "FTDI" ){
            omronPortName = info.portName();
        }
    }
    LogMsg ("--------------");
}

void MainWindow::write2SerialPort(const QString &msg)
{
    const QString temp = msg + "\n";
    omron->write(temp.toStdString().c_str());
    LogMsg("write : " + msg);
}

QString MainWindow::readFromSerialPort()
{
    QByteArray read = omron->readAll();
    LogMsg("ANS = " + QString(read));
    return QString(read);
}
