#include "calibrationwindow.h"
#include "ui_calibrationwindow.h"
#include "config.h"
#include "mainwindow.h"

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#include <opencv2/objdetect/aruco_board.hpp>
#include <iostream>

#include <QMessageBox>

CalibrationWindow::CalibrationWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CalibrationWindow)
{
    ui->setupUi(this);

    ui->label_camera->setScaledContents(true);

    ui->btn_continue->setEnabled(false);

    cap.open(0, cv::CAP_DSHOW);

    timer = new QTimer(this);

    connect(timer, &QTimer::timeout,
            this, &CalibrationWindow::updateCamera);

    timer->start(30);
}

CalibrationWindow::~CalibrationWindow()
{
    if(timer)
        timer->stop();

    if(cap.isOpened())
        cap.release();

    delete ui;
}

/////////// mon //////////////////

////////////// oat //////////////
