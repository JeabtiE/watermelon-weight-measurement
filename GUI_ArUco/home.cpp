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
    CalibrationWindow *calWindow = new CalibrationWindow();

    calWindow->show();

    this->close();
}
