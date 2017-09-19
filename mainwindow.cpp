#include "mainwindow.h"
#include "ui_mainwindow.h"

const QString DESKTOP_PATH = QStandardPaths::locate(QStandardPaths::DesktopLocation, QString(), QStandardPaths::LocateDirectory);
const QString DATA_PATH = DESKTOP_PATH + "Temp_Record";

enum Respond_Type{
    temp = 1,
    SV = 2,
    other = 3
};

enum E5CC_Address{
    // QByteArray::fromHex(QString::number(E5CC_Address::setPoint, 16).toStdString().c_str()
    PV = 0x0000,
    PV2= 0x0404,
    setPoint = 0x0106,
    alarmType = 0x0108,
    alarmUpperLimit = 0x010A,
    alarmLowerLimit = 0x010C,
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    msgCount = 0;
    tempControlOnOff = false;
    tempRecordOnOff = false;
    modbusReady = true;
    tempDecimal = 0.1; // for 0.1

    //Check Temp Directory, is not exist, create
    QDir myDir;
    myDir.setPath(DATA_PATH); if( !myDir.exists()) myDir.mkpath(DATA_PATH);

    plot = ui->plot;
    plot->xAxis->setLabel("Time");
    plot->yAxis->setLabel("Temp. [C]");
    plot->addGraph();
    plot->graph(0)->setPen(QPen(Qt::blue));
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("MM/dd HH:mm:ss");
    plot->xAxis->setTicker(dateTicker);
    plot->xAxis2->setVisible(true);
    plot->yAxis2->setVisible(true);
    plot->xAxis2->setTicks(false);
    plot->yAxis2->setTicks(false);
    plot->xAxis2->setTickLabels(false);
    plot->yAxis2->setTickLabels(false);
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

    atComboxEnable = false;
    ui->comboBox_AT->addItem("AT cancel");
    ui->comboBox_AT->addItem("100% AT execute");
    ui->comboBox_AT->addItem("40% AT execute");
    ui->comboBox_AT->setCurrentIndex(0);
    atComboxEnable = true;

    findSeriesPortDevices();
    omron = NULL;

    panalOnOff(false);
    ui->pushButton_Control->setEnabled(false);
    ui->pushButton_RecordTemp->setEnabled(false);

}

MainWindow::~MainWindow()
{
    if (omron) omron->disconnectDevice();

    delete plot;
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
        //LogMsg("PortName     ="+info.portName() );
        //LogMsg("description  ="+(!info.description().isEmpty() ?  info.description() : blankString) );
        //LogMsg("manufacturer ="+(!info.manufacturer().isEmpty() ? info.manufacturer() : blankString) );
        //LogMsg("serialNumber ="+(!info.serialNumber().isEmpty() ? info.serialNumber() : blankString) );
        //LogMsg("Location     ="+info.systemLocation()  );
        //LogMsg("Vendor       ="+(info.vendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : blankString)  );
        //LogMsg("Identifier   ="+(info.productIdentifier() ? QString::number(info.productIdentifier(), 16) : blankString));
        //LogMsg("=======================");

        LogMsg(info.portName() + ", " + info.serialNumber() + ", " + info.manufacturer());

        ui->comboBox_SeriesNumber->addItem( info.serialNumber(), (QString) info.portName());

        //if(info.serialNumber() == "FT0875RPA" && info.manufacturer() == "FTDI" ){
        //    omronPortName = info.portName();
        //    qDebug() << omronPortName;
        //}
    }
    LogMsg ("--------------");
}

void MainWindow::showDataUnit(QModbusDataUnit unit)
{
    qDebug() << "Type :" << unit.registerType();
    qDebug() << "Start Address" << unit.startAddress();
    qDebug() << "number of values" << unit.valueCount();
    qDebug() << unit.values();
    //for( int i = 0; i < unit.valueCount(); i++ ){
    //    qDebug() << i << " = " << unit.value(i);
    //}
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
    //ui->checkBox_EnableSend->setChecked(!IO);

    ui->lineEdit_Cmd->setEnabled(IO);
    ui->lineEdit_SV->setEnabled(IO);
    ui->checkBox_EnableSend->setEnabled(IO);
    ui->checkBox_RunSop->setEnabled(IO);
    ui->comboBox_Func->setEnabled(IO);
    ui->comboBox_AT->setEnabled(IO);
    ui->pushButton_ReadRH->setEnabled(IO);
    ui->pushButton_AskTemp->setEnabled(IO);
    ui->pushButton_SetSV->setEnabled(IO);
    ui->spinBox_TempRecordTime->setEnabled(IO);
    ui->spinBox_TempStableTime->setEnabled(IO);
    ui->spinBox_DeviceAddress->setEnabled(IO);
    ui->doubleSpinBox_TempTorr->setEnabled(IO);
    ui->doubleSpinBox_TempStepSize->setEnabled(IO);
    ui->pushButton_OpenFile->setEnabled(IO);
}

void MainWindow::on_pushButton_AskTemp_clicked()
{
    askTemperature();
    waitForMSec(1000);
    askSetPoint();
}

void MainWindow::read(QModbusDataUnit::RegisterType type, quint16 adress, int size, int respondFlag)
{
    // accesing Hodling register, address 0x0000, for 2 Byte of value.
    QModbusDataUnit ans(type, adress, size);
    respondType = respondFlag;
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
        if(respondType == Respond_Type::temp){
            temperature = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString tempStr = tr("Current Temperature : %1 C").arg(QString::number(temperature));
            ui->lineEdit_Temp->setText(QString::number(temperature) + " C");
            LogMsg(tempStr);
        }else if(respondType == Respond_Type::SV){
            SV = QString::number(unit.value(1), 10).toDouble() * tempDecimal;
            QString svStr = tr("Current Set Point : %1 C").arg(QString::number(SV));
            ui->lineEdit_CurrentSV->setText(QString::number(SV) + " C");
            LogMsg(svStr);
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
                                    arg(reply->rawResult().exceptionCode(), -1, 16), 5000);
    } else {
        statusBar()->showMessage(tr("Read response error: %1 (code: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->error(), -1, 16), 5000);
    }

    reply->deleteLater();
    waitForMSec(1000);
    modbusReady = true;
}

void MainWindow::askTemperature(int waitTime)
{
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::PV, 2, Respond_Type::temp);
    //while(!modbusReady){
    //    waitForMSec(waitTime);
    //    qDebug() << "waiting for temp reading ....";
    //}
}

void MainWindow::askSetPoint(int waitTime)
{
    read(QModbusDataUnit::HoldingRegisters, E5CC_Address::setPoint, 2, Respond_Type::SV);
    //while(!modbusReady){
    //    waitForMSec(waitTime);
    //    qDebug() << "waiting for SV reading ....";
    //}
}

void MainWindow::askRunStop(int waitTime)
{

}

void MainWindow::askAT(int waitTime)
{

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

void MainWindow::write(int address, int value)
{
    if (!omron) return;
    statusBar()->clearMessage();

    modbusReady = false;
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters);
    writeUnit.setValueCount(1);
    writeUnit.setStartAddress(address);
    writeUnit.setValue(0, value);

    qDebug() << QString::number(writeUnit.startAddress(), 16);
    qDebug() << writeUnit.value(0);

    //for (uint i = 0; i < writeUnit.valueCount(); i++) {
    //    if (table == QModbusDataUnit::Coils)
    //        writeUnit.setValue(i, writeModel->m_coils[i + writeUnit.startAddress()]);
    //    else
    //        writeUnit.setValue(i, writeModel->m_holdingRegisters[i + writeUnit.startAddress()]);
    //}

    if (auto *reply = omron->sendWriteRequest(writeUnit, omronID)) {
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
        statusBar()->showMessage(tr("Write error: ") + omron->errorString(), 5000);
    }

}

void MainWindow::request(QModbusPdu::FunctionCode code, QByteArray cmd)
{
    modbusReady = false;
    QModbusPdu pdu;
    pdu.setFunctionCode(code);
    pdu.setData(cmd);
    qDebug() << pdu; //TODO show in LogMsg

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
        //while(!modbusReady){
        //    waitForMSec(100);
        //    qDebug() << "waiting for request done....";
        //}
    }else{
        LogMsg("(No Send) PDU = 0x " + formatHex( static_cast<int>(regType), 2) + " "+ input);
    }
}

void MainWindow::on_pushButton_SetSV_clicked()
{
    statusBar()->clearMessage();
    double sv = ui->lineEdit_SV->text().toDouble();
    double sv_input = sv / tempDecimal;
    int sv_2 = (qint16) (sv_input + 0.5);
    qDebug() << sv_input << "," << sv_2;
    QString valueStr = formatHex(sv_2, 8);
    QString addressStr = formatHex(E5CC_Address::setPoint, 4);

    QString cmd = addressStr + " 00 02 04" + valueStr;
    QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
    request(QModbusPdu::WriteMultipleRegisters, value);

}

void MainWindow::on_pushButton_Control_clicked()
{
    tempControlOnOff = !tempControlOnOff;
    panalOnOff(!tempControlOnOff);
    ui->pushButton_RecordTemp->setEnabled(!tempControlOnOff);

    if(tempControlOnOff) {
        LogMsg("================ Temperature control =====");
        ui->pushButton_Control->setStyleSheet("background-color: rgb(0,255,0)");
    }else{
        LogMsg("================ Temperature control Off =====");
        ui->pushButton_Control->setStyleSheet("");
        qDebug()  << "temp control. = " << tempControlOnOff;
        return;
    }

    if(tempControlOnOff){

        //Looping ========================
        const double sv = ui->lineEdit_SV->text().toDouble();
        const int tempGetTime = ui->spinBox_TempRecordTime->value() * 1000;
        const int tempStableTime = ui->spinBox_TempStableTime->value() * 60 * 1000;
        const double tempTorr = ui->doubleSpinBox_TempTorr->value();
        const double tempStepSize = ui->doubleSpinBox_TempStepSize->value();

        askTemperature();
        LogMsg("Target Temperature          : " + QString::number(sv) + " C.");

        //const int tempStep = qAbs(temperature - sv) / tempStepSize;
        const int direction = (temperature > sv ) ? -1 : 1;
        //LogMsg("Temperature step : " + QString::number(tempStep));

        // set output file =================
        QString fileName = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_tempControl_ID="+ QString::number(omronID) +".dat";
        QString filePath = DATA_PATH + "/" + fileName;
        LogMsg("data save to : " + filePath);
        QFile outfile(filePath);

        outfile.open(QIODevice::WriteOnly);
        QTextStream stream(&outfile);
        QString lineout;

        lineout.sprintf("###%s", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss\n").toStdString().c_str());
        stream << lineout;
        lineout = "###Target Temperature          : " + QString::number(sv) + " C.\n";
        stream << lineout;
        lineout.sprintf("###%11s,\t%12s,\t%10s\n", "Date", "Date_t", "temp [C]");
        stream << lineout;

        timeData.clear();
        while(tempControlOnOff){
            //Set SV
            double sp = sv;
            if(qAbs(temperature-sv) >= tempStepSize){
                sp = temperature + direction * tempStepSize  ;
            }
            double sp_input = sp / tempDecimal;
            int sp_2 = (qint16) (sp_input + 0.5);
            qDebug() << sp_input << "," << sp_2;
            QString valueStr = formatHex(sp_2, 8);
            QString addressStr = formatHex(E5CC_Address::setPoint, 4);

            QString cmd = addressStr + " 00 02 04" + valueStr;
            QByteArray value = QByteArray::fromHex(cmd.toStdString().c_str());
            request(QModbusPdu::WriteMultipleRegisters, value);
            ui->lineEdit_CurrentSV->setText(QString::number(sp) + " C");
            LogMsg("========= try to go to " + QString::number(sp) + " C.");

            int count = 0;
            do{
                qDebug()  << "temp control. do-loop 1 = " << tempControlOnOff;
                if(!tempControlOnOff) break;

                askTemperature();

                QDateTime date = QDateTime::currentDateTime();
                QCPGraphData plotdata;
                plotdata.key = date.toTime_t();
                plotdata.value = temperature;

                lineout.sprintf("%14s,\t%12.0f,\t%10f\n", date.toString("MM-dd HH:mm:ss").toStdString().c_str(), plotdata.key, plotdata.value);
                stream << lineout;

                timeData.push_back(plotdata);
                plot->graph(0)->data()->clear();
                plot->graph(0)->data()->add(timeData);
                plot->yAxis->rescale();
                plot->xAxis->rescale();

                double ymin = plot->yAxis->range().lower - 2;
                double ymax = plot->yAxis->range().upper + 2;

                plot->yAxis->setRangeLower(ymin);
                plot->yAxis->setRangeUpper(ymax);

                plot->replot();

                waitForMSec(tempGetTime-500);

                if( qAbs(temperature - sp) <= tempTorr ){
                    count += tempGetTime;
                    LogMsg( " temperature stable for : " +  QString::number(count/1000.) + " sec. |"
                            +QString::number(sp) + " - " + QString::number(temperature) + "| < "  + QString::number(tempTorr) + " C");
                }else{
                    if(count > 0){
                        LogMsg( " temperature over-shoot. reset stable counter.");
                    }
                    count = 0;
                }

            }while( count < tempStableTime  && tempControlOnOff ); // if temperature stable for 10 min

            if( qAbs(temperature - sv)< tempTorr) {
                LogMsg("=========== Target Temperature Reached =============");
                break;
            }

        }

        //=========== now is the E5CC control
        //only measure temperature
        while(tempControlOnOff){
            qDebug()  << "temp control. do-loop 2 = " << tempControlOnOff;
            askTemperature();

            QDateTime date = QDateTime::currentDateTime();
            QCPGraphData plotdata;
            plotdata.key = date.toTime_t();
            plotdata.value = temperature;

            lineout.sprintf("%14s\t%12.0f\t%10.1f\n", date.toString("MM-dd HH:mm:ss").toStdString().c_str(), plotdata.key, plotdata.value);
            stream << lineout;

            timeData.push_back(plotdata);
            plot->graph(0)->data()->clear();
            plot->graph(0)->data()->add(timeData);
            plot->yAxis->rescale();
            plot->xAxis->rescale();

            double ymin = plot->yAxis->range().lower - 2;
            double ymax = plot->yAxis->range().upper + 2;

            plot->yAxis->setRangeLower(ymin);
            plot->yAxis->setRangeUpper(ymax);

            plot->replot();

            waitForMSec(tempGetTime-500);

        };

        stream << "###============ end of file ==============";
        outfile.close();

        panalOnOff(true);
        ui->pushButton_Control->setStyleSheet("");
        tempControlOnOff = false;
    }
}

void MainWindow::on_comboBox_AT_currentIndexChanged(int index)
{
    if(!atComboxEnable) return;
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
    //ui->spinBox_TempRecordTime->setEnabled(!tempRecordOnOff);
    ui->pushButton_Control->setEnabled(!tempRecordOnOff);
    panalOnOff(!tempRecordOnOff);

    if(tempRecordOnOff){
        LogMsg("===================== Recording temperature Start.");
        ui->pushButton_RecordTemp->setStyleSheet("background-color: rgb(0,255,0)");
        ui->lineEdit_CurrentSV->setText("see device."); // disble for not requesting too often.
    }else{
        LogMsg("===================== Recording temperature Stopped.");
        ui->pushButton_RecordTemp->setStyleSheet("");
    }

    if( tempRecordOnOff){
        const int tempGetTime = ui->spinBox_TempRecordTime->value() * 1000;

        // set output file =================
        QString fileName = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_tempRecord_ID="+ QString::number(omronID) +".dat";
        QString filePath = DATA_PATH + "/" + fileName;
        LogMsg("data save to : " + filePath);
        QFile outfile(filePath);

        outfile.open(QIODevice::WriteOnly);
        QTextStream stream(&outfile);
        QString lineout;

        lineout.sprintf("###%s", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss\n").toStdString().c_str());
        stream << lineout;
        lineout = "###Temperature Record.\n";
        stream << lineout;
        lineout.sprintf("###%11s,\t%12s,\t%10s\n", "Date", "Date_t", "temp [C]");
        stream << lineout;

        timeData.clear();

        //only measure temperature
        while(tempRecordOnOff){
            qDebug()  << "Recording Temp. = " << tempControlOnOff;
            askTemperature(500);

            QDateTime date = QDateTime::currentDateTime();
            QCPGraphData plotdata;
            plotdata.key = date.toTime_t();
            plotdata.value = temperature;
            //plotdata.value = qrand();

            lineout.sprintf("%14s,\t%12.0f,\t%10.1f\n", date.toString("MM-dd HH:mm:ss").toStdString().c_str(), plotdata.key, plotdata.value);
            stream << lineout;
            outfile.flush(); // write to file immedinately, but seem not working...

            timeData.push_back(plotdata);
            plot->graph(0)->data()->clear();
            plot->graph(0)->data()->add(timeData);
            plot->yAxis->rescale();
            plot->xAxis->rescale();

            double ymin = plot->yAxis->range().lower - 2;
            double ymax = plot->yAxis->range().upper + 2;

            plot->yAxis->setRangeLower(ymin);
            plot->yAxis->setRangeUpper(ymax);

            plot->replot();

            waitForMSec(tempGetTime-500);

        };

        stream << "###============ end of file ==============";
        outfile.close();

    }

}

void MainWindow::on_pushButton_ReadRH_clicked()
{
    bool ok = false;
    quint16 address = ui->lineEdit_Cmd->text().toUInt(&ok,16);
    read(QModbusDataUnit::HoldingRegisters, address, 2, Respond_Type::other);
}

void MainWindow::on_spinBox_DeviceAddress_valueChanged(int arg1)
{
    omronID = arg1;
}

void MainWindow::on_pushButton_OpenFile_clicked()
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

    timeData.clear();
    while(stream.readLineInto(&line)){
        if( line.left(3) == "###") continue;

        QStringList list = line.split(",");
        if(list.size() != 3) continue;
        QString time = list[1];
        QString data = list[2];
        QCPGraphData plotdata;
        plotdata.key = time.toInt();
        plotdata.value = data.toDouble();

        timeData.push_back(plotdata);
    }

    infile.close();

    plot->graph(0)->data()->clear();
    plot->graph(0)->data()->add(timeData);
    plot->yAxis->rescale();
    plot->xAxis->rescale();

    double ymin = plot->yAxis->range().lower - 2;
    double ymax = plot->yAxis->range().upper + 2;

    plot->yAxis->setRangeLower(ymin);
    plot->yAxis->setRangeUpper(ymax);

    plot->replot();

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
    omron->setTimeout(2000);
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

        askTemperature();
        waitForMSec(1000);
        askSetPoint();

    }else{
        ui->textEdit_Log->setTextColor(QColor(255,0,0,255));
        LogMsg("The Omron temperature control cannot be found on any COM port.");
        ui->textEdit_Log->setTextColor(QColor(0,0,0,255));
        ui->comboBox_SeriesNumber->setEnabled(true);
        ui->pushButton_Connect->setStyleSheet("");
    }

}
