#include "home.h"
#include "ui_home.h"
#include "calibrationwindow.h"

Home::Home(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Home)
{
    ui->setupUi(this);
    connect(ui->btn_Start, &QPushButton::clicked, this, &Home::on_btn_Start_clicked);}

Home::~Home()
{
    delete ui;
}

void Home::on_btn_Start_clicked()
{
    // 1. สร้าง Instance ของหน้าถัดไป (CalibrationWindow)
    CalibrationWindow *calWindow = new CalibrationWindow();

    // 2. สั่งให้หน้าถัดไปแสดงผล
    calWindow->show();

    // 3. ปิดหน้า Home ปัจจุบันทิ้งไป
    this->close();
}
