#include "mainwindow.h"
#include "ui_mainwindow.h"

const QString DESKTOP_PATH = QStandardPaths::locate(QStandardPaths::DesktopLocation, QString(), QStandardPaths::LocateDirectory);
const QString DATA_PATH_2 = DESKTOP_PATH + "Temp_Record";
const QString DATA_PATH = "Z:/triplet/Temp_Record";

enum E5CC_Address{
    // QByteArray::fromHex(QString::number(E5CC_Address::setPoint, 16).toStdString().c_str()
    PV = 0x0000,
    MV= 0x0008,
    SV = 0x0106,

    MVupper = 0x0A0A,
    MVlower = 0x0A0C,

    PID_P=0x0A00,
    PID_I=0x0A02,
    PID_D=0x0A04
};

enum timing{
    modbus = 100,
    getTempTimer = 10,
    clockUpdate = 50,
    timeUp = 1000*60*10,
    timeOut = 700
};

//TODO nicer time display on LogMsg

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    msgCount = 0;
    tempControlOnOff = false;
    tempRecordOnOff = false;
    modbusReady = true;
    spinBoxEnable = false;
    muteLog = false;
    tempDecimal = 0.1; // for 0.1

    //======= clock
    clock = new QTimer(this);
    clock->stop();
    connect(clock, SIGNAL(timeout()), this, SLOT(showTime()));
    totalElapse.setHMS(0,0,0,0);
    dayCounter = 0;
    checkDay = false;
    waitTimer = new QTimer(this);
    waitTimer->stop();
    waitTimer->setSingleShot(false);
    connect(waitTimer, SIGNAL(timeout()), this, SLOT(allowSetNextSV()));

    helpDialog = new QDialog(this);
    HelpLabel = new QLabel();
    helpDialog->setWindowTitle("Help");
    QImage image(":fig1.PNG");
    picNumber = 1;
    HelpLabel->setPixmap(QPixmap::fromImage(image));

    QPushButton * next = new QPushButton("Next");
    connect(next, SIGNAL(pressed()) , this, SLOT(HelpPicNext()));

    QVBoxLayout *mainLayout = new QVBoxLayout(helpDialog);
    mainLayout->addWidget(HelpLabel);
    mainLayout->addWidget(next);

    //Check Temp Directory, is not exist, create
    QDir myDir;
    myDir.setPath(DATA_PATH);
    // if Z:/triplet/Temp_Record does not exist, make directory on desktop.
    if( !myDir.exists()) {
        QMessageBox msgBox;
        msgBox.setText("Cannot locate Z Drive!\n"
                       "Please set the NAS to be Z:\n"
                       "Data will be saved in Desktop/Temp_Record");
        myDir.mkpath(DATA_PATH_2);
    }else{
        LogMsg("Data will be saved in : " + DATA_PATH );
    }
    plot = ui->plot;
    plot->xAxis->setLabel("Time");
    plot->yAxis->setLabel("Temp. [C]");
    plot->addGraph(plot->xAxis, plot->yAxis2);
    plot->graph(0)->setName("Output");
    plot->graph(0)->setPen(QPen(Qt::darkGreen)); // MV
    plot->addGraph();
    plot->graph(1)->setName("Temp.");
    plot->graph(1)->setPen(QPen(Qt::blue)); // PV
    plot->addGraph();
    plot->graph(2)->setName("Set-temp");
    plot->graph(2)->setPen(QPen(Qt::red)); // SV
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("MM/dd HH:mm:ss");
    plot->xAxis->setTicker(dateTicker);
    plot->xAxis2->setVisible(true);
    plot->yAxis2->setVisible(true);
    plot->yAxis2->setLabel(" Output [%]");
    plot->yAxis2->setRangeLower(0.0);
    plot->xAxis2->setTicks(false);
    plot->yAxis2->setTicks(true);
    plot->xAxis2->setTickLabels(false);
    plot->yAxis2->setTickLabels(true);
    plot->setInteraction(QCP::iRangeZoom,true);
    plot->setInteraction(QCP::iRangeDrag,true);
    plot->axisRect()->setRangeDrag(Qt::Vertical);
    plot->axisRect()->setRangeZoom(Qt::Vertical);
    plot->legend->setVisible(true);
    //set legend position
    //plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
    plot->axisRect()->setAutoMargins(QCP::msLeft | QCP::msTop | QCP::msBottom);
    plot->axisRect()->setMargins(QMargins(0,0,150,0));
    plot->axisRect()->insetLayout()->setInsetPlacement(0, QCPLayoutInset::ipFree);
    plot->axisRect()->insetLayout()->setInsetRect(0, QRectF(1.1,0,0.1,0.1));

    plot->replot();

    ui->comboBox_Func->addItem("0x00 Invalid", QModbusPdu::Invalid);
    ui->comboBox_Func->addItem("0x01 Read Coils", QModbusPdu::ReadCoils);
    ui->comboBox_Func->addItem("0x02 Read Discrte Inputs", QModbusPdu::ReadDiscreteInputs);
    ui->comboBox_Func->addItem("0x03 Read Holding Registers", QModbusPdu::ReadHoldingRegisters);
    ui->comboBox_Func->addItem("0x04 Read Input Registers", QModbusPdu::ReadInputRegisters);
    ui->comboBox_Func->addItem("0x05 Write Single Coil", QModbusPdu::WriteSingleCoil);
    ui->comboBox_Func->addItem("0x06 Write Single Register", QModbusPdu::WriteSingleRegister);
    ui->comboBox_Func->addItem("0x07 Read Exception Status", QModbusPdu::ReadExceptionStatus);
    ui->comboBox_Func->addItem("0x08 Diagonstics", QModbusPdu::Diagnostics);
    ui->comboBox_Func->addItem("0x0B Get Comm Event Counter", QModbusPdu::GetCommEventCounter);
    ui->comboBox_Func->addItem("0x0C Get Comm Event Log", QModbusPdu::GetCommEventLog);
    ui->comboBox_Func->addItem("0x0f Write Multiple Coils", QModbusPdu::WriteMultipleCoils);
    ui->comboBox_Func->addItem("0x10 Write Multiple Registers", QModbusPdu::WriteMultipleRegisters);
    ui->comboBox_Func->addItem("0x11 Report Server ID", QModbusPdu::ReportServerId);
    ui->comboBox_Func->addItem("0x14 Read File Record", QModbusPdu::ReadFileRecord);
    ui->comboBox_Func->addItem("0x15 Write File Record", QModbusPdu::WriteFileRecord);
    ui->comboBox_Func->addItem("0x16 Mask Write Register", QModbusPdu::MaskWriteRegister);
    ui->comboBox_Func->addItem("0x17 Read Write Multiple Registers", QModbusPdu::ReadWriteMultipleRegisters);
    ui->comboBox_Func->addItem("0x18 Read Fifo Queue", QModbusPdu::ReadFifoQueue);
    ui->comboBox_Func->addItem("0x2B Encapsulated Interface Transport", QModbusPdu::EncapsulatedInterfaceTransport);
    ui->comboBox_Func->addItem("0x100 Undefined Function Code", QModbusPdu::UndefinedFunctionCode);

    comboxEnable = false;
    ui->comboBox_AT->addItem("AT cancel");
    ui->comboBox_AT->addItem("100% AT execute");
    ui->comboBox_AT->addItem("40% AT execute");
    ui->comboBox_AT->setCurrentIndex(0);

    ui->comboBox_Mode->addItem("Stable", 1);
    ui->comboBox_Mode->addItem("Fixed time", 2);
    ui->comboBox_Mode->addItem("Fixed rate", 3);
    ui->comboBox_Mode->addItem("Normal + Fixed rate", 4);

    ui->comboBox_Mode->setItemData(0, QBrush(Qt::black), Qt::TextColorRole);
    ui->comboBox_Mode->setItemData(1, QBrush(Qt::red), Qt::TextColorRole);
    ui->comboBox_Mode->setItemData(2, QBrush(Qt::blue), Qt::TextColorRole);
    ui->comboBox_Mode->setItemData(3, QBrush(Qt::darkGreen), Qt::TextColorRole);

    findSeriesPortDevices();
    omron = NULL;

    panalOnOff(false);
    ui->pushButton_Control->setEnabled(false);
    ui->pushButton_RecordTemp->setEnabled(false);

    //============= some useful addresses
    ui->comboBox_MemAddress->addItem("0x0000 (opt) PV "                     , 0x0000);
    ui->comboBox_MemAddress->addItem("0x0008 (opt) MV heating "             , 0x0008);
    ui->comboBox_MemAddress->addItem("0x000A (opt) MV cooling "            , 0x000A);
    ui->comboBox_MemAddress->addItem("0x0106 (opt) SP "                     , 0x0106);
    ui->comboBox_MemAddress->addItem("0x0108 (opt) Alarm 1 type "           , 0x0108);
    ui->comboBox_MemAddress->addItem("0x010A (opt) Alarm 1 UL "             , 0x010A);
    ui->comboBox_MemAddress->addItem("0x010C (opt) Alarm 1 LL "             , 0x010C);
    ui->comboBox_MemAddress->addItem("0x010E (opt) Alarm 2 type "           , 0x010E);
    ui->comboBox_MemAddress->addItem("0x0110 (opt) Alarm 2 UL "             , 0x0110);
    ui->comboBox_MemAddress->addItem("0x0112 (opt) Alarm 2 LL "             , 0x0112);
    ui->comboBox_MemAddress->addItem("0x0608 (opt) heater current 1 "       , 0x0608);
    ui->comboBox_MemAddress->addItem("0x060A (opt) MV heating "             , 0x060A);
    ui->comboBox_MemAddress->addItem("0x060C (opt) MV cooling "             , 0x060C);
    ui->comboBox_MemAddress->addItem("0x0702 (opt) Prop. band "             , 0x0702);
    ui->comboBox_MemAddress->addItem("0x0704 (opt) Inte. time "             , 0x0704);
    ui->comboBox_MemAddress->addItem("0x0706 (opt) deri. time "             , 0x0706);

    ui->comboBox_MemAddress->addItem("0x071E (adj) MV at stop "             , 0x071E);
    ui->comboBox_MemAddress->addItem("0x0722 (adj) MV at PV Error "         , 0x0722);
    ui->comboBox_MemAddress->addItem("0x0A0A (adj) Prop. band "             , 0x0A00);
    ui->comboBox_MemAddress->addItem("0x0A02 (adj) Inte. time "             , 0x0A02);
    ui->comboBox_MemAddress->addItem("0x0A04 (adj) deri. time "             , 0x0A04);
    ui->comboBox_MemAddress->addItem("0x0A0A (adj) MV upper limit "         , 0x0A0A);
    ui->comboBox_MemAddress->addItem("0x0A0C (adj) MV lower limit "         , 0x0A0C);

    ui->comboBox_MemAddress->addItem("0x0710 (ini) Ctrl. period heating "   , 0x0710);
    ui->comboBox_MemAddress->addItem("0x0712 (ini) Ctrl. period cooling "   , 0x0712);
    ui->comboBox_MemAddress->addItem("0x0D06 (ini) Ctrl. output 1 current " , 0x0D06);
    ui->comboBox_MemAddress->addItem("0x0D08 (ini) Ctrl. output 2 current " , 0x0D08);
    ui->comboBox_MemAddress->addItem("0x0D1E (ini) SP upper limit "         , 0x0D1E);
    ui->comboBox_MemAddress->addItem("0x0D20 (ini) SP lower limit "         , 0x0D20);
    ui->comboBox_MemAddress->addItem("0x0D22 (ini) Std heating/cooling "    , 0x0D22);
    ui->comboBox_MemAddress->addItem("0x0D24 (ini) Direct/Reverse opt. "    , 0x0D24);
    ui->comboBox_MemAddress->addItem("0x0D28 (ini) PID on/off "             , 0x0D28);

    ui->comboBox_MemAddress->addItem("0x0500 (protect) Opt/Adj protect "       , 0x0500);
    ui->comboBox_MemAddress->addItem("0x0502 (protect) Init/Comm protect "     , 0x0502);
    ui->comboBox_MemAddress->addItem("0x0504 (protect) Setting Chg. protect "  , 0x0504);
    ui->comboBox_MemAddress->addItem("0x0506 (protect) PF key protect "        , 0x0506);

    ui->comboBox_MemAddress->addItem("0x0E0C (adv) Ctrl. output 1 Assignment "   , 0x0E0C);
    ui->comboBox_MemAddress->addItem("0x0E0E (adv) Ctrl. output 2 Assignment "   , 0x0E0E);
    ui->comboBox_MemAddress->addItem("0x0E20 (adv) Aux. output 1 Assignment "    , 0x0E20);
    ui->comboBox_MemAddress->addItem("0x0E22 (adv) Aux. output 2 Assignment "    , 0x0E22);
    ui->comboBox_MemAddress->addItem("0x0E24 (adv) Aux. output 3 Assignment "    , 0x0E24);

    comboxEnable = true;

    ui->textEdit_Log->setTextColor(QColor(34,139,34,255));
    LogMsg("The AT and RUN/STOP do not get from the device. Please be careful.");
    ui->textEdit_Log->setTextColor(QColor(0,0,0,255));

}

MainWindow::~MainWindow()
{
    if (omron) omron->disconnectDevice();

    clock->stop();
    waitTimer->stop();

    delete waitTimer;
    delete clock;
    delete plot;
    delete omron;
    delete ui;
}


void MainWindow::LogMsg(QString str, bool newLine)
{
    if( muteLog ) return;
    if( newLine == false ){
        ui->textEdit_Log->moveCursor(QTextCursor::End);
        ui->textEdit_Log->insertPlainText(str);
    }else{
        msgCount ++;
        QString dateStr = QDateTime::currentDateTime().toString("HH:mm:ss ");
        QString countStr;
        countStr.sprintf("[%05d]: ", msgCount);
        str.insert(0, countStr).insert(0, dateStr);
        ui->textEdit_Log->append(str);
    }
    int max = ui->textEdit_Log->verticalScrollBar()->maximum();
    ui->textEdit_Log->verticalScrollBar()->setValue(max);
}

void MainWindow::findSeriesPortDevices()
{
    LogMsg("-------------- COM Ports found :");
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        //LogMsg("PortName     ="+info.portName() );
        //LogMsg("description  ="+(!info.description().isEmpty() ?  info.description() : "") );
        //LogMsg("manufacturer ="+(!info.manufacturer().isEmpty() ? info.manufacturer() : "") );
        //LogMsg("serialNumber ="+(!info.serialNumber().isEmpty() ? info.serialNumber() : "") );
        //LogMsg("Location     ="+info.systemLocation()  );
        //LogMsg("Vendor       ="+(info.vendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : "")  );
        //LogMsg("Identifier   ="+(info.productIdentifier() ? QString::number(info.productIdentifier(), 16) : ""));
        //LogMsg("=======================");
        LogMsg(info.portName() + ", " + info.serialNumber() + ", " + info.manufacturer());
        ui->comboBox_SeriesNumber->addItem( info.serialNumber(), (QString) info.portName());
    }
    LogMsg ("--------------");
}

void MainWindow::keyPressEvent(QKeyEvent *key)
{
    if(key->key() == Qt::Key_Shift ){
        plot->axisRect()->setRangeDrag(Qt::Horizontal);
        plot->axisRect()->setRangeZoom(Qt::Horizontal);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *key)
{
    if(key->key() == Qt::Key_Shift ){
        plot->axisRect()->setRangeDrag(Qt::Vertical);
        plot->axisRect()->setRangeZoom(Qt::Vertical);
    }
}

QString MainWindow::formatHex(int value, int digit)
{
    QString valueStr = QString::number(value, 16).toUpper();
    while(valueStr.size() < digit){
        valueStr.insert(0, "0");
    }
    return valueStr;
}

void MainWindow::waitForMSec(int msec)
{
    //wait for waitTime
    QEventLoop eventLoop;
    QTimer::singleShot(msec, &eventLoop, SLOT(quit()));
    eventLoop.exec();
}

void MainWindow::panalOnOff(bool IO)
{
    ui->lineEdit_Cmd->setEnabled(IO);
    ui->lineEdit_SV->setEnabled(IO);
    ui->checkBox_EnableSend->setEnabled(IO);
    ui->checkBox_RunSop->setEnabled(IO);
    ui->comboBox_Func->setEnabled(IO);
    ui->comboBox_AT->setEnabled(IO);
    ui->pushButton_ReadRH->setEnabled(IO);
    ui->pushButton_AskStatus->setEnabled(IO);
    ui->pushButton_GetPID->setEnabled(IO);
    ui->pushButton_SetSV->setEnabled(IO);
    ui->spinBox_TempRecordTime->setEnabled(IO);
    ui->spinBox_TempStableTime->setEnabled(IO);
    ui->spinBox_DeviceAddress->setEnabled(IO);
    ui->doubleSpinBox_TempTorr->setEnabled(IO);
    ui->doubleSpinBox_TempStepSize->setEnabled(IO);
    ui->doubleSpinBox_MVlower->setEnabled(IO);
    ui->doubleSpinBox_MVupper->setEnabled(IO);
    ui->comboBox_Mode->setEnabled(IO);
    ui->checkBox_MuteLogMsg->setEnabled(IO);
    ui->comboBox_MemAddress->setEnabled(IO);
    ui->lineEdit_SV2->setEnabled(IO);
    ui->doubleSpinBox_SV2WaitTime->setEnabled(IO);
}

void MainWindow::showTime()
{
    double hour = totalElapse.elapsed()/1000./60./60.;
    if( checkDay && hour <= 22){
        dayCounter ++;
        checkDay = false;
    }
    if(hour > 22){
        checkDay = true;
    }
    QTime t(0,0,0,0);
    t = t.addMSecs(totalElapse.elapsed());
    ui->lineEdit_clock->setText(QString::number(dayCounter) + "/" + t.toString("HH:mm:ss:zzz"));
    //qDebug() << "==========" << t.msec();
}

void MainWindow::allowSetNextSV()
{
    nextSV = true;
}

void MainWindow::on_pushButton_AskStatus_clicked()
{
    askTemperature();
    int i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }

    askSetPoint();
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }

    askMV();
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
}

void MainWindow::read(QModbusDataUnit::RegisterType type, quint16 adress, int size)
{
    // accesing Hodling register, address 0x0000, for 2 Byte of value.
    statusBar()->clearMessage();
    QModbusDataUnit ans(type, adress, size);
    respondType = adress;
    modbusReady = false;

    if (auto *reply = omron->sendReadRequest(ans, omronID)) {
        if (!reply->isFinished()){
            connect(reply, &QModbusReply::finished, this, &MainWindow::readReady);
        }else{
            delete reply; // broadcast replies return immediately
        }
    } else {
        statusBar()->showMessage(tr("Read error: ") + omron->errorString(), 0);
    }
}

void MainWindow::readReady()
{
    //LogMsg("------ reading, " + QString::number(respondType));
    auto reply = qobject_cast<QModbusReply *>(sender());

    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        if(respondType == E5CC_Address::PV){
            temperature = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString str = tr("Current Temperature : %1 C").arg(QString::number(temperature));
            ui->lineEdit_Temp->setText(QString::number(temperature) + " C");
            LogMsg(str);
        }else if(respondType == E5CC_Address::SV){
            SV = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString str = tr("Current Set Point : %1 C").arg(QString::number(SV));
            ui->lineEdit_CurrentSV->setText(QString::number(SV) + " C");
            LogMsg(str);
        }else if(respondType == E5CC_Address::MV){
            MV = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString str = tr("Current MV : %1 \%").arg(QString::number(MV));
            ui->lineEdit_CurrentMV->setText(QString::number(MV) + " %");
            LogMsg(str);
        }else if(respondType == E5CC_Address::MVupper){
            MVupper = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString str = tr("MV upper limit : %1 \%").arg(QString::number(MVupper));
            ui->doubleSpinBox_MVupper->setValue(MVupper);
            LogMsg(str);
            plot->yAxis2->setRangeUpper(MVupper + 2);
            plot->replot();
        }else if(respondType == E5CC_Address::MVlower){
            MVlower = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString str = tr("MV lower limit : %1 \%").arg(QString::number(MVlower));
            ui->doubleSpinBox_MVlower->setValue(MVlower);
            LogMsg(str);
        }else if(respondType == E5CC_Address::PID_P){
            pid_P = QString::number(unit.value(1), 10).toDouble() * 0.1;
            QString str = tr("P       : %1 ").arg(QString::number(pid_P));
            ui->lineEdit_P->setText(QString::number(pid_P));
            LogMsg(str);
        }else if(respondType == E5CC_Address::PID_I){
            pid_I = QString::number(unit.value(1), 10).toDouble();
            QString str = tr("I (raw) : %1 sec").arg(QString::number(pid_I));
            ui->lineEdit_I->setText(QString::number(pid_I));
            LogMsg(str);
        }else if(respondType == E5CC_Address::PID_D){
            pid_D = QString::number(unit.value(1), 10).toDouble();
            QString str = tr("D (raw) : %1 sec").arg(QString::number(pid_D));
            ui->lineEdit_D->setText(QString::number(pid_D));
            LogMsg(str);
        }else{
            LogMsg("respond count: " + QString::number(unit.valueCount()));
            for (uint i = 0; i < unit.valueCount(); i++) {
                const QString entry = tr("Address: %1, Value: %2").arg(unit.startAddress()).arg(QString::number(unit.value(i), 10));
                LogMsg(entry);
            }
        }

    } else if (reply->error() == QModbusDevice::ProtocolError) {
        statusBar()->showMessage(tr("Read response error: %1 (Mobus exception: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->rawResult().exceptionCode(), -1, 16), 0);
    } else {
        statusBar()->showMessage(tr("Read response error: %1 (code: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->error(), -1, 16), 0);
    }

    reply->deleteLater();
    modbusReady = true;
}

void MainWindow::askTemperature()
{
    LogMsg("------ get Temperature.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PV, 2);
}

void MainWindow::askSetPoint()
{
    LogMsg("------ get Set Point.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::SV, 2);
}

void MainWindow::askMV()
{
    LogMsg("------ get MV.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::MV, 2);
}

void MainWindow::askMVupper()
{
    LogMsg("------ get MV upper.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::MVupper, 2);
}

void MainWindow::askMVlower()
{
    LogMsg("------ get MV lower.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::MVlower, 2);
}

void MainWindow::getSetting()
{
    askTemperature();
    int i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }

    askSetPoint();
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }

    askMV();
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }

    //ask MV upper Limit
    spinBoxEnable = false;
    askMVupper();
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
    askMVlower();
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
    spinBoxEnable = true;

    //get PID constant
    LogMsg("------ get Propertion band.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PID_P, 2);
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
    LogMsg("------ get integration time.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PID_I, 2);
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
    LogMsg("------ get derivative time.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PID_D, 2);
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }


}

void MainWindow::setAT(int atFlag)
{
    statusBar()->clearMessage();
    QString cmd;
    if( atFlag == 0){
        cmd = "00 00 03 00";
        ui->lineEdit_SV->setEnabled(true);
        ui->pushButton_SetSV->setEnabled(true);
        LogMsg("Set AT to none.");
    }else if( atFlag == 1){
        cmd = "00 00 03 01";
        ui->lineEdit_SV->setEnabled(false);
        ui->pushButton_SetSV->setEnabled(false);
        LogMsg("Set AT to 100%., disable Set Point.");
    }else if( atFlag == 2){
        cmd = "00 00 03 02";
        ui->lineEdit_SV->setEnabled(false);
        ui->pushButton_SetSV->setEnabled(false);
        LogMsg("Set AT to 40%. disable Set Point.");
    }
    QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
    request(QModbusPdu::WriteSingleRegister, value);
}

void MainWindow::setSV(double SV)
{
    statusBar()->clearMessage();
    int sv_input = (qint16) (SV / tempDecimal + 0.5);
    QString valueStr = formatHex(sv_input, 8);
    QString addressStr = formatHex(E5CC_Address::SV, 4);

    QString cmd = addressStr + " 00 02 04" + valueStr;
    QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
    request(QModbusPdu::WriteMultipleRegisters, value);
}

void MainWindow::request(QModbusPdu::FunctionCode code, QByteArray cmd)
{
    statusBar()->clearMessage();
    modbusReady = false;
    QModbusPdu pdu;
    pdu.setFunctionCode(code);
    pdu.setData(cmd);
    qDebug() << pdu;

    QModbusRequest ask(pdu);

    if (auto *reply = omron->sendRawRequest(ask, omronID)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::ProtocolError) {
                    statusBar()->showMessage(tr("Write response error: %1 (Mobus exception: 0x%2)")
                        .arg(reply->errorString()).arg(reply->rawResult().exceptionCode(), -1, 16), 0);
                } else if (reply->error() != QModbusDevice::NoError) {
                    statusBar()->showMessage(tr("Write response error: %1 (code: 0x%2)").
                        arg(reply->errorString()).arg(reply->error(), -1, 16), 0);
                }
                reply->deleteLater();
                modbusReady = true;
            });
        } else {
            // broadcast replies return immediately
            reply->deleteLater();
        }

    } else {
        statusBar()->showMessage(tr("Write error: ") + omron->errorString(), 0);
    }

}

void MainWindow::on_lineEdit_Cmd_returnPressed()
{
    statusBar()->clearMessage();
    QString input = ui->lineEdit_Cmd->text();
    QByteArray value = QByteArray::fromHex(input.toStdString().c_str());
    QModbusPdu::FunctionCode regType = static_cast<QModbusPdu::FunctionCode> (ui->comboBox_Func->currentData().toInt());
    if( ui->checkBox_EnableSend->isChecked()){
        LogMsg("(Send) PDU = 0x " + formatHex( static_cast<int>(regType), 2) + " "+ input);
        request(regType, value);
    }else{
        LogMsg("(No Send) PDU = 0x " + formatHex( static_cast<int>(regType), 2) + " "+ input);
    }
}

void MainWindow::on_pushButton_SetSV_clicked()
{
    setSV( ui->lineEdit_SV->text().toDouble());
}

void MainWindow::on_pushButton_Control_clicked()
{
    tempControlOnOff = !tempControlOnOff;
    panalOnOff(!tempControlOnOff);
    ui->actionOpen_File->setEnabled(!tempControlOnOff);
    ui->pushButton_RecordTemp->setEnabled(!tempControlOnOff);
    //muteLog = ui->checkBox_MuteLogMsg->isChecked();

    if(tempControlOnOff) {
        LogMsg("================ Temperature control =====");
        ui->pushButton_Control->setStyleSheet("background-color: rgb(0,255,0)");
    }else{
        LogMsg("================ Temperature control Off =====");
        ui->pushButton_Control->setStyleSheet("");
        on_comboBox_Mode_currentIndexChanged(ui->comboBox_Mode->currentIndex());
        qDebug()  << "temp control. = " << tempControlOnOff;
        clock->stop();
        totalElapse.setHMS(0,0,0,0);
        return;
    }

    double iniTemp = 0;
    if(tempControlOnOff){
        on_pushButton_AskStatus_clicked();
        iniTemp = temperature;
        LogMsg("Current Temperature         : " + QString::number(temperature) + " C.");

        const double targetValue = ui->lineEdit_SV->text().toDouble();
        const int tempGetTime = ui->spinBox_TempRecordTime->value() * 1000; // msec
        int tempWaitTime = ui->spinBox_TempStableTime->value() * 60 * 1000; // msec
        const double tempTorr = ui->doubleSpinBox_TempTorr->value();
        const double tempStepSize = ui->doubleSpinBox_TempStepSize->value();
        const int mode = ui->comboBox_Mode->currentData().toInt();
        const double targetValue_2 = ui->lineEdit_SV2->text().toDouble();
        const double targetValue_2_waitTime = ui->doubleSpinBox_SV2WaitTime->value() * 60.*1000.;
        if( mode == 4){
            tempWaitTime = tempWaitTime * tempStepSize;
        }

        if( mode == 4){
            LogMsg("First go to : " + QString::number(targetValue_2) + " C. normaly.");
            LogMsg("Then go to  : " + QString::number(targetValue) + " C. at fixed rate.");
        }else{
            LogMsg("Target Temperature          : " + QString::number(targetValue) + " C.");
        }

        //============= estimate total time
        double estTransitionTime = 5; //min
        if( mode == 2 || mode == 3 || mode == 4) estTransitionTime = 0;
        double estSlope = (estTransitionTime + tempWaitTime/60/1000) / tempStepSize ;
        double estTotalTime = estSlope * qAbs(temperature-targetValue);
        if(mode == 4){
            estSlope = ui->spinBox_TempStableTime->value(); // min/C
            estTotalTime = estSlope * qAbs(targetValue_2-targetValue);
        }

        QMessageBox box;
        QString boxMsg;
        if( mode == 1){
            LogMsg("======== Stable Mode ==========");
            boxMsg.sprintf("======== Stable Mode ========== \n"
                           "Estimated transition time : %6.1f min. \n"
                           "Estimated gradience       : %6.1f min/C \n"
                           "Estimated total time      : %6.1f min = %6.1f hr",
                           estTransitionTime,
                           estSlope,
                           estTotalTime, estTotalTime/60.);
        }else if(mode == 2){
            LogMsg("======== Fixed Time Mode ==========");
            boxMsg.sprintf("======== Fixed Time Mode ========== \n"
                           "Estimated gradience  : %6.1f min/C \n"
                           "Estimated total time : %6.1f min = %6.1f hr",
                           estSlope,
                           estTotalTime, estTotalTime/60.);
        }else if(mode == 3){
            LogMsg("======== Fixed Rate Mode ==========");
            boxMsg.sprintf("======== Fixed Rate Mode ========== \n"
                           "Set-temp Gradience   : %6.1f min/C \n"
                           "Estimated total time : %6.1f min = %6.1f hr",
                           estSlope,
                           estTotalTime, estTotalTime/60.);
        }else if(mode == 4){
            LogMsg("======== Normal + Fixed Rate Mode ==========");
            boxMsg.sprintf("======== Normal + Fixed Rate Mode ========== \n"
                           "1) Go to %5.1f C using normal mode.\n"
                           "   Time unknown. \n"
                           "2) Fixed rate to go to %5.1f C\n"
                           "   Fixed-rate Set-temp Gradience   : %6.1f min/C \n"
                           "   Estimated fixed-rate time       : %6.1f min = %6.1f hr",
                           targetValue_2, targetValue,
                           estSlope,
                           estTotalTime, estTotalTime/60.);
        }
        LogMsg("Estimated transiton Time : " + QString::number(estTransitionTime) + " min.");
        LogMsg("Estimated gradience      : " + QString::number(estSlope) + " min/C.");
        LogMsg("Estimated total Time     : " + QString::number(estTotalTime) + " min. = " + QString::number(estTotalTime/60.) + " hr.");
        box.setText(boxMsg);
        box.setInformativeText("Do you want to proceed ?");
        box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);
        if(box.exec() == QMessageBox::Cancel) {
            tempControlOnOff = false;
            ui->pushButton_Control->setStyleSheet("");
            panalOnOff(true);
            ui->actionOpen_File->setEnabled(true);
            ui->pushButton_RecordTemp->setEnabled(true);
            on_comboBox_Mode_currentIndexChanged(ui->comboBox_Mode->currentIndex());
            LogMsg("=============== Slow Temperature control cancelled.======");
            return;
        }

        // set output file =================
        QDateTime startTime = QDateTime::currentDateTime();
        QString fileName = startTime.toString("yyyyMMdd_HHmmss")
                + "_tempControl_mode" + QString::number(mode)
                + "_"  + ui->comboBox_SeriesNumber->currentText() +".dat";
        QString filePath = DATA_PATH + "/" + fileName;
        LogMsg("data save to : " + filePath);
        QFile outfile(filePath);

        outfile.open(QIODevice::WriteOnly| QIODevice::Text);
        QTextStream stream(&outfile);
        QString lineout;

        lineout.sprintf("###%s", startTime.toString("yyyy-MM-dd HH:mm:ss\n").toStdString().c_str());
        stream << lineout;
        if( mode == 1){
            lineout = "### Control mode          :  Stable Temperature.\n";
        }else if(mode == 2){
            lineout = "### Control mode          :  Fixed Time.\n";
        }else if(mode == 3){
            lineout = "### Control mode          :  Set-temp Fixed Rate. \n";
        }else if(mode == 4){
            lineout = "### Control mode          :  Normal + Set-temp Fixed Rate. \n";
        }
        stream << lineout;
        if( mode == 1){
            lineout = "### Target Temperature          : " + QString::number(targetValue) + " C.\n";
            stream << lineout;
            lineout = "### Temperature  stable time    : " + QString::number(tempWaitTime) + " min.\n";
            stream << lineout;
            lineout = "### Temperature tolerance       : " + QString::number(tempTorr) + " C.\n";
            stream << lineout;
        }else if(mode == 2){
            lineout = "### Target Temperature      : " + QString::number(targetValue) + " C.\n";
            stream << lineout;
            lineout = "### Set-temp change time    : " + QString::number(tempWaitTime) + " min.\n";
            stream << lineout;
        }else if(mode == 3){
            lineout = "### Target Temperature      : " + QString::number(targetValue) + " C.\n";
            stream << lineout;
            lineout = "### Set-temp change rate    : " + QString::number(tempWaitTime/60./1000.) + " min/C.\n";
            stream << lineout;
        }else if(mode == 4){
            lineout = "### Set-temp of normal      : " + QString::number(targetValue_2) + " C.\n";
            stream << lineout;
            lineout = "### Set-temp of fixed rate  : " + QString::number(targetValue) + " C.\n";
            stream << lineout;
            lineout = "### Set-temp change rate    : " + QString::number(tempWaitTime/60./1000.) + " min/C.\n";
            stream << lineout;
        }
        lineout.sprintf("###%11s,\t%12s,\t%10s,\t%10s,\t%10s\n", "Date", "Date_t", "temp [C]", "SV [C]", "Output [%]");
        stream << lineout;
        stream.flush();

        clock->setSingleShot(false);
        clock->start(timing::clockUpdate);
        totalElapse.start(); // used for clock
        QTimer getTempTimer;
        getTempTimer.setSingleShot(true);

        //########################### mode 4 extra code, go to targetValue_2
        //----- set SV
        setSV(targetValue_2);

        //----- wait for temp reach targetValue_2, while recording temperature.
        pvData.clear();
        svData.clear();
        mvData.clear();
        muteLog = ui->checkBox_MuteLogMsg->isChecked();
        bool targetValue_2_Reached = false;
        bool waitTimerStarted = false;
        waitTimer->setSingleShot(true);
        while(tempControlOnOff && mode == 4 && !targetValue_2_Reached){
            getTempTimer.start(tempGetTime);
            askTemperature();
            int i = 0;
            while(!modbusReady) {
                i++;
                waitForMSec(timing::modbus);
                if( i > 10 ){
                    modbusReady = true;
                }
            }
            askMV();
            i = 0;
            while(!modbusReady) {
                i++;
                waitForMSec(timing::modbus);
                if( i > 10 ){
                    modbusReady = true;
                }
            }

            QDateTime date = QDateTime::currentDateTime();
            fillDataAndPlot(date, temperature, targetValue_2, MV);

            lineout.sprintf("%14s,\t%12d,\t%10.1f,\t%10.1f,\t%10.1f\n",
                            date.toString("MM-dd HH:mm:ss").toStdString().c_str(),
                            date.toTime_t(),
                            temperature,
                            targetValue_2,
                            MV);
            stream << lineout;
            stream.flush();

            while(getTempTimer.remainingTime() > 0 ){
                waitForMSec(timing::getTempTimer);

                if(waitTimer->remainingTime() <= 0 && waitTimerStarted == true){
                    targetValue_2_Reached = true; // break the getTemp loop

                    muteLog = false;
                    LogMsg("Target Set-temp stable. Start fixed rate. Elapse time : " + QString::number(totalElapse.elapsed()/1000./60) + " mins.");
                    muteLog = ui->checkBox_MuteLogMsg->isChecked();
                    lineout.sprintf("### fixed-rate start.\n");
                    stream << lineout;
                    stream.flush();

                    break; // break this loop
                }
            }

            if(temperature == targetValue_2 && waitTimerStarted == false){
                waitTimer->start(targetValue_2_waitTime);
                waitTimerStarted = true;

                muteLog = false;
                LogMsg("Target Set-temp reached : " + QString::number(targetValue_2) + " C. Elapse time : " + QString::number(totalElapse.elapsed()/1000./60) + " mins.");
                LogMsg("wait for 10 mins.");
                muteLog = ui->checkBox_MuteLogMsg->isChecked();
                lineout.sprintf("### Target Set-temp reached : %5.1f C\n", targetValue_2);
                stream << lineout;
                stream.flush();
            }

        }
        muteLog = false;

        //Looping ========================
        askTemperature();
        int i = 0;
        while(!modbusReady) {
            i++;
            waitForMSec(timing::modbus);
            if( i > 10 ){
                modbusReady = true;
            }
        }
        double smallShift = temperature;
        LogMsg("Present Temperature : " + QString::number(temperature) + " C.");
        const int direction = (temperature > targetValue ) ? (-1) : 1;
        LogMsg("Temperature step            : " + QString::number(direction * tempStepSize) + " C.");

        if( mode == 1){
            waitTimer->setSingleShot(true);
        }else{
            waitTimer->setSingleShot(false);
            waitTimer->start(tempWaitTime);
            waitTimerStarted = true;
        }
        while(tempControlOnOff){
            nextSV = false;
            if( mode == 1) {
                waitTimer->stop();
                waitTimerStarted = false;
            }
            //----------------Set SV
            if( mode == 1 || mode == 2 ){
                if(direction * (targetValue - temperature) >= tempStepSize){
                    // when direction is +1, when temperature smaller than tempStepSize, incrase smallshift by a step size.
                    smallShift = temperature + direction * tempStepSize  ;
                }else{
                    //else, smallshift = target value.
                    smallShift = targetValue;
                }
            }

            if( mode == 3 || mode == 4){
                if(direction * (targetValue - smallShift) >= tempStepSize){
                    smallShift = smallShift + direction * tempStepSize  ;
                }else{
                    smallShift = targetValue;
                }
            }

            ui->lineEdit_CurrentSV->setText(QString::number(smallShift) + " C");
            LogMsg("==== Set-temp : " + QString::number(smallShift) + " C. Elapse Time : " + QString::number(totalElapse.elapsed()/1000./60.) + " mins.");

            setSV(smallShift);

            muteLog = ui->checkBox_MuteLogMsg->isChecked();
            do{
                getTempTimer.start(tempGetTime);
                qDebug()  << "temp control. do-loop 1 = " << tempControlOnOff;
                if(!tempControlOnOff) break;

                askTemperature();
                int i = 0;
                while(!modbusReady) {
                    i++;
                    waitForMSec(timing::modbus);
                    if( i > 10 ){
                        modbusReady = true;
                    }
                }
                askMV();
                i = 0;
                while(!modbusReady) {
                    i++;
                    waitForMSec(timing::modbus);
                    if( i > 10 ){
                        modbusReady = true;
                    }
                }

                QDateTime date = QDateTime::currentDateTime();
                fillDataAndPlot(date, temperature, smallShift, MV);

                lineout.sprintf("%14s,\t%12d,\t%10.1f,\t%10.1f,\t%10.1f\n",
                                date.toString("MM-dd HH:mm:ss").toStdString().c_str(),
                                date.toTime_t(),
                                temperature,
                                smallShift,
                                MV);
                stream << lineout;
                stream.flush();

                while(getTempTimer.remainingTime() > 0 ){
                    waitForMSec(timing::getTempTimer);
                    if( nextSV == true){
                        break;
                    }
                }
                muteLog=false;
                //TODO under friendly display
                if(waitTimerStarted){
                    LogMsg(" x", false);
                }else{
                    LogMsg(" /", false);
                }
                //LogMsg(" - " + QString::number(fixedTime.elapsed()/1000.), false);
                muteLog = ui->checkBox_MuteLogMsg->isChecked();

                if( mode == 1){ //========== for stable mode
                    if( qAbs(temperature - smallShift) <= tempTorr){
                        if( waitTimerStarted == false){
                            waitTimer->start(tempWaitTime);
                            waitTimerStarted = true;
                        }
                    }else{
                        waitTimer->stop();
                        waitTimerStarted = false;
                        nextSV = false;
                    }
                    if( waitTimerStarted == true){
                        double stableTime = (tempWaitTime - waitTimer->remainingTime())/1000.;
                        LogMsg( " temperature stable for : " +  QString::number(stableTime) + " sec. |"
                         + QString::number(smallShift) + " - " + QString::number(temperature) + "| < "  + QString::number(tempTorr) + " C");
                    }
                }

            }while( !nextSV  && tempControlOnOff );
            muteLog = false;

            if (smallShift == targetValue){
                if( mode == 1 ) {
                    lineout = "###=========== Target Temperature Reached =============";
                }else if( mode == 2 || mode == 3 || mode == 4) {
                    lineout = "###=========== Time Up  =============";
                }
                stream << lineout;
                stream.flush();
                LogMsg(lineout);
                break;
            }
        }

        double totalTime = totalElapse.elapsed() /1000./60. + dayCounter * 24. * 60.; // min
        LogMsg("Total time : " + QString::number(totalTime) + " mins = " + QString::number(totalTime/60.) + " hours.");
        double tempChanged = qAbs(iniTemp - temperature);
        LogMsg("Average gradience : " + QString::number(totalTime/tempChanged) + " min/C." );

        //=========== now is the E5CC control
        //only measure temperature
        muteLog = true;
        while(tempControlOnOff){
            getTempTimer.start(tempGetTime);
            qDebug()  << "temp control. do-loop 2 = " << tempControlOnOff;
            askTemperature();
            int i = 0;
            while(!modbusReady) {
                i++;
                waitForMSec(timing::modbus);
                if( i > 10 ){
                    modbusReady = true;
                }
            }
            askMV();
            i = 0;
            while(!modbusReady) {
                i++;
                waitForMSec(timing::modbus);
                if( i > 10 ){
                    modbusReady = true;
                }
            }

            QDateTime date = QDateTime::currentDateTime();
            fillDataAndPlot(date, temperature, smallShift, MV);

            lineout.sprintf("%14s,\t%12d,\t%10.1f,\t%10.1f,\t%10.1f\n",
                            date.toString("MM-dd HH:mm:ss").toStdString().c_str(),
                            date.toTime_t(),
                            temperature,
                            smallShift,
                            MV);
            stream << lineout;
            stream.flush(); // write to file

            while(getTempTimer.remainingTime() != -1 ){
                waitForMSec(timing::getTempTimer);
            }

        };
        muteLog = false;

        stream << "###============ end of file ==============";
        outfile.close();
    }

}

void MainWindow::on_comboBox_AT_currentIndexChanged(int index)
{
    if(!comboxEnable) return;
    setAT(index);
}

void MainWindow::on_checkBox_RunSop_clicked()
{
    statusBar()->clearMessage();
    QString cmd;
    if( ui->checkBox_RunSop->isChecked() ){
        cmd = "00 00 01 00";
        LogMsg("Set Run.");
    }else{
        cmd = "00 00 01 01";
        LogMsg("Set Stop.");
    }
    QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
    request(QModbusPdu::WriteSingleRegister, value);
}

void MainWindow::on_pushButton_RecordTemp_clicked()
{
    tempRecordOnOff = !tempRecordOnOff;
    ui->pushButton_Control->setEnabled(!tempRecordOnOff);
    ui->actionOpen_File->setEnabled(!tempRecordOnOff);
    panalOnOff(!tempRecordOnOff);

    if(tempRecordOnOff){
        LogMsg("===================== Recording temperature Start.");
        ui->pushButton_RecordTemp->setStyleSheet("background-color: rgb(0,255,0)");
        ui->lineEdit_CurrentSV->setText("see device."); // disble for not requesting too often.
    }else{
        LogMsg("===================== Recording temperature Stopped.");
        ui->pushButton_RecordTemp->setStyleSheet("");
        clock->stop();
        totalElapse.setHMS(0,0,0,0);
    }

    //plot->graph(1)->data()->clear();

    if( tempRecordOnOff){
        clock->setSingleShot(false);
        clock->start(timing::clockUpdate);
        totalElapse.start();
        const int tempGetTime = ui->spinBox_TempRecordTime->value() * 1000;
        askSetPoint();
        int i = 0;
        while(!modbusReady) {
            i++;
            waitForMSec(timing::modbus);
            if( i > 10 ){
                modbusReady = true;
            }
        }

        // set output file =================
        QDateTime startTime = QDateTime::currentDateTime();
        QString fileName = startTime.toString("yyyyMMdd_HHmmss") +
                "_tempRecord_" + ui->comboBox_SeriesNumber->currentText() +".dat";
        QString filePath = DATA_PATH + "/" + fileName;
        LogMsg("data save to : " + filePath);
        QFile outfile(filePath);

        outfile.open(QIODevice::WriteOnly| QIODevice::Text);
        QTextStream stream(&outfile);
        QString lineout;

        lineout.sprintf("###%s", startTime.toString("yyyy-MM-dd HH:mm:ss\n").toStdString().c_str());
        stream << lineout;
        lineout = "###Temperature Recording.\n";
        stream << lineout;
        lineout.sprintf("###%11s,\t%12s,\t%10s,\t%10s,\t%10s\n", "Date", "Date_t", "temp [C]", "SV [C]", "Output [%]");
        stream << lineout;

        pvData.clear();
        svData.clear();
        mvData.clear();
        //only measure temperature
        muteLog = ui->checkBox_MuteLogMsg->isChecked();
        QTimer getTempTimer;
        getTempTimer.setSingleShot(true);
        while(tempRecordOnOff){
            getTempTimer.start(tempGetTime);
            askTemperature();
            int i = 0;
            while(!modbusReady) {
                i++;
                waitForMSec(timing::modbus);
                if( i > 10 ){
                    modbusReady = true;
                }
            }
            askMV();
            i = 0;
            while(!modbusReady) {
                i++;
                waitForMSec(timing::modbus);
                if( i > 10 ){
                    modbusReady = true;
                }
            }

            QDateTime date = QDateTime::currentDateTime();
            fillDataAndPlot(date, temperature, SV, MV);

            lineout.sprintf("%14s,\t%12d,\t%10.1f,\t%10.1f,\t%10.1f\n",
                            date.toString("MM-dd HH:mm:ss").toStdString().c_str(),
                            date.toTime_t(),
                            temperature,
                            (double) SV,
                            (double) MV);
            stream << lineout;
            stream.flush();

            while(getTempTimer.remainingTime() != -1 ){
                waitForMSec(timing::getTempTimer);
            }
        };
        muteLog = false;

        stream << "###============ end of file ==============";
        outfile.close();

    }

    clock->stop();
}

void MainWindow::on_pushButton_ReadRH_clicked()
{
    bool ok = false;
    quint16 address = ui->lineEdit_Cmd->text().toUInt(&ok,16);
    read(QModbusDataUnit::HoldingRegisters, address, 2);
}

void MainWindow::on_pushButton_Connect_clicked()
{
    QString omronPortName = ui->comboBox_SeriesNumber->currentData().toString();
    LogMsg("=========== setting modbus.");
    omronID = ui->spinBox_DeviceAddress->value();
    omron = new QModbusRtuSerialMaster(this);
    omron->setConnectionParameter(QModbusDevice::SerialPortNameParameter, omronPortName);
    omron->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, QSerialPort::Baud9600);
    omron->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    omron->setConnectionParameter(QModbusDevice::SerialParityParameter, QSerialPort::NoParity);
    omron->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::TwoStop);
    omron->setTimeout(timing::timeOut);
    omron->setNumberOfRetries(0);

    if(omron->connectDevice()){
        ui->textEdit_Log->setTextColor(QColor(0,0,255,255));
        LogMsg("The Omron temperature control is connected in " + omronPortName + ".");
        ui->textEdit_Log->setTextColor(QColor(0,0,0,255));

        ui->comboBox_SeriesNumber->setEnabled(false);
        ui->pushButton_Connect->setStyleSheet("background-color: rgb(255,127,80)");
        ui->pushButton_Connect->setEnabled(false);
        panalOnOff(true);
        ui->pushButton_Control->setEnabled(true);
        ui->pushButton_RecordTemp->setEnabled(true);
        ui->lineEdit_SV2->setEnabled(false);
        ui->doubleSpinBox_SV2WaitTime->setEnabled(false);

        QString title = this->windowTitle();
        this->setWindowTitle(title + " | " + ui->comboBox_SeriesNumber->currentText());

        getSetting();

    }else{
        ui->textEdit_Log->setTextColor(QColor(255,0,0,255));
        LogMsg("The Omron temperature control cannot be connected on any COM port.");
        ui->textEdit_Log->setTextColor(QColor(0,0,0,255));
        ui->comboBox_SeriesNumber->setEnabled(true);
        ui->pushButton_Connect->setStyleSheet("");
    }

}

void MainWindow::on_doubleSpinBox_MVlower_valueChanged(double arg1)
{
    if(!spinBoxEnable) return;
    if(!modbusReady) return;

    MVlower = arg1;

    int sv_2 = (qint16) (arg1 / tempDecimal + 0.5);
    qDebug() << sv_2;
    QString valueStr = formatHex(sv_2, 8);
    QString addressStr = formatHex(E5CC_Address::MVlower, 4);

    QString cmd = addressStr + " 00 02 04" + valueStr;
    QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
    request(QModbusPdu::WriteMultipleRegisters, value);

}

void MainWindow::on_doubleSpinBox_MVupper_valueChanged(double arg1)
{
    if(!spinBoxEnable) return;
    if(!modbusReady) return;

    MVupper = arg1;

    int sv_2 = (qint16) (arg1 / tempDecimal + 0.5);
    qDebug() << sv_2;
    QString valueStr = formatHex(sv_2, 8);
    QString addressStr = formatHex(E5CC_Address::MVupper, 4);

    QString cmd = addressStr + " 00 02 04" + valueStr;
    QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
    request(QModbusPdu::WriteMultipleRegisters, value);

    plot->yAxis2->setRangeLower(MVupper + 2);
    plot->replot();
}

void MainWindow::on_pushButton_GetPID_clicked()
{
    //get PID constant
    LogMsg("------ get Propertion band.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PID_P, 2);
    int i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
    LogMsg("------ get integration time.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PID_I, 2);
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
    LogMsg("------ get derivative time.");
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PID_D, 2);
    i = 0;
    while(!modbusReady) {
        i++;
        waitForMSec(timing::modbus);
        if( i > 10 ){
            modbusReady = true;
        }
    }
}

void MainWindow::on_comboBox_Mode_currentIndexChanged(int index)
{
    if(!comboxEnable) return;
    if(index == 1){
        ui->doubleSpinBox_TempTorr->setEnabled(false);
        ui->doubleSpinBox_TempStepSize->setEnabled(true);
        ui->doubleSpinBox_TempStepSize->setValue(0.5);
        ui->comboBox_Mode->setStyleSheet("color: #FF0000");
        ui->label_TimeStable->setStyleSheet("color: #FF0000");
        ui->label_TimeStable->setText("Set-temp changes [min] :");
        ui->label_SV_2->setStyleSheet("");
        ui->lineEdit_SV2->setEnabled(false);
        ui->doubleSpinBox_SV2WaitTime->setEnabled(false);
        ui->label_SV2WaitTime->setStyleSheet("");
    }else if(index == 0){
        ui->doubleSpinBox_TempTorr->setEnabled(true);
        ui->doubleSpinBox_TempStepSize->setEnabled(true);
        ui->doubleSpinBox_TempStepSize->setValue(0.5);
        ui->comboBox_Mode->setStyleSheet("");
        ui->label_TimeStable->setStyleSheet("");
        ui->label_TimeStable->setText("Temp. stable for [min] :");
        ui->label_SV_2->setStyleSheet("");
        ui->lineEdit_SV2->setEnabled(false);
        ui->doubleSpinBox_SV2WaitTime->setEnabled(false);
        ui->label_SV2WaitTime->setStyleSheet("");
    }else if(index == 2){
        ui->doubleSpinBox_TempTorr->setEnabled(false);
        ui->doubleSpinBox_TempStepSize->setEnabled(false);
        ui->doubleSpinBox_TempStepSize->setValue(0.1);
        ui->comboBox_Mode->setStyleSheet("color: #0000FF");
        ui->label_TimeStable->setStyleSheet("color: #0000FF");
        ui->label_TimeStable->setText("Set-temp rate [min/C] :");
        ui->label_SV_2->setStyleSheet("");
        ui->lineEdit_SV2->setEnabled(false);
        ui->doubleSpinBox_SV2WaitTime->setEnabled(false);
        ui->label_SV2WaitTime->setStyleSheet("");
    }else if( index == 3){
        ui->doubleSpinBox_TempTorr->setEnabled(false);
        ui->doubleSpinBox_TempStepSize->setEnabled(false);
        ui->doubleSpinBox_TempStepSize->setValue(0.1);
        ui->comboBox_Mode->setStyleSheet("color: #006325");
        ui->label_TimeStable->setStyleSheet("color: #006325");
        ui->label_TimeStable->setText("Set-temp rate [min/C] :");
        ui->lineEdit_SV2->setEnabled(true);
        ui->label_SV_2->setStyleSheet("color: #006325");
        ui->lineEdit_SV2->setText("92");
        ui->doubleSpinBox_SV2WaitTime->setEnabled(true);
        ui->label_SV2WaitTime->setStyleSheet("color: #006325");
        ui->doubleSpinBox_SV2WaitTime->setValue(10.);
    }
}

void MainWindow::on_checkBox_MuteLogMsg_clicked(bool checked)
{
    if ( checked ){
        muteLog = true;
    }else{
        muteLog = false;
    }
}

void MainWindow::on_comboBox_MemAddress_currentTextChanged(const QString &arg1)
{
    if(!comboxEnable) return;
    quint16 address = ui->comboBox_MemAddress->currentData().toUInt();
    LogMsg("--------- read " + arg1);
    read(QModbusDataUnit::HoldingRegisters, address, 2);
}

void MainWindow::on_actionOpen_File_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", DATA_PATH );
    QFile infile(filePath);

    if(infile.open(QIODevice::ReadOnly | QIODevice::Text)){
        LogMsg("Open File : %s" + filePath);
    }else{
        LogMsg("Open file failed ");
        return;
    }

    QTextStream stream(&infile);
    QString line;

    pvData.clear();
    svData.clear();
    mvData.clear();
    bool haveSVMVData = false;
    while(stream.readLineInto(&line)){
        if( line.left(3) == "###") {
            LogMsg(line);
            continue;
        }
        QStringList list = line.split(",");
        if(list.size() < 3) continue;
        QString time = list[1];
        QString pv = list[2];
        QCPGraphData plotdata;
        plotdata.key = time.toInt();
        plotdata.value = pv.toDouble();
        pvData.push_back(plotdata);

        if( list.size() <= 5){
            haveSVMVData = true;
            QString sv = list[3];
            QString mv = list[4];
            plotdata.value = sv.toDouble();
            svData.push_back(plotdata);

            plotdata.value = mv.toDouble();
            mvData.push_back(plotdata);
        }
    }

    infile.close();

    plot->graph(0)->data()->clear();
    plot->graph(1)->data()->clear();
    plot->graph(2)->data()->clear();

    plot->graph(1)->data()->add(pvData);
    if( haveSVMVData ){
        plot->graph(2)->data()->add(svData);
        plot->graph(0)->data()->add(mvData);
    }

    plot->yAxis->rescale();
    plot->yAxis2->rescale();
    plot->xAxis->rescale();

    double ymin = plot->yAxis->range().lower - 2;
    double ymax = plot->yAxis->range().upper + 2;
    plot->yAxis->setRangeLower(ymin);
    plot->yAxis->setRangeUpper(ymax);

    ymax = plot->yAxis2->range().upper + 2;
    plot->yAxis2->setRangeLower(0);
    plot->yAxis2->setRangeUpper(ymax);

    plot->replot();
}

void MainWindow::fillDataAndPlot(const QDateTime date, const double PV, const double SV, const double MV)
{
    QCPGraphData plotdata;
    plotdata.key = date.toTime_t();

    plotdata.value = PV;
    pvData.push_back(plotdata);
    plotdata.value = SV;
    svData.push_back(plotdata);
    plotdata.value = MV;
    mvData.push_back(plotdata);

    plot->graph(0)->data()->clear();
    plot->graph(0)->data()->add(mvData);
    plot->graph(1)->data()->clear();
    plot->graph(1)->data()->add(pvData);
    plot->graph(2)->data()->clear();
    plot->graph(2)->data()->add(svData);
    plot->yAxis->rescale();
    plot->xAxis->rescale();

    double ymin = plot->yAxis->range().lower - 2;
    double ymax = plot->yAxis->range().upper + 2;

    plot->yAxis->setRangeLower(ymin);
    plot->yAxis->setRangeUpper(ymax);

    plot->replot();

}

void MainWindow::on_actionHelp_Page_triggered()
{
    if( helpDialog->isHidden() ){
        helpDialog->show();
    }
}

void MainWindow::HelpPicNext()
{
    if( picNumber == 2){
        QImage image(":fig1.PNG");
        HelpLabel->setPixmap(QPixmap::fromImage(image));
        picNumber = 1;
        return;
    }

    if( picNumber == 1){
        QImage image(":fig2.PNG");
        HelpLabel->setPixmap(QPixmap::fromImage(image));
        picNumber ++;
        return;
    }
}
