#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <QTimer>
#include <QImage>
#include <QPixmap>

#include <vector>
#include <cmath>

using namespace cv;
using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // เปิดกล้อง
    cap.open(0, cv::CAP_DSHOW);

    // timer สำหรับกล้อง
    QTimer *cameraTimer = new QTimer(this);
    connect(cameraTimer, &QTimer::timeout, this, &MainWindow::updateCamera);
    cameraTimer->start(30);   // 30 ms = ~30 FPS

    // timer สำหรับบันทึกข้อมูล
    saveTimer = new QTimer(this);
    connect(saveTimer, &QTimer::timeout, this, &MainWindow::saveWatermelon);

    // timer สำหรับนับถอยหลัง
    countdownTimer = new QTimer(this);
    connect(countdownTimer, &QTimer::timeout, this, [=](){

        countdown--;

        if(countdown <= 0)
        {
            saveWatermelon();
            countdown = 10;
        }

    });

    countdownTimer->start(1000); // 1 วินาที

    ui->tableWidget->setColumnCount(4);
    ui->tableWidget->setHorizontalHeaderLabels({"NO :","Width","Height","Weight"});

    ui->tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

MainWindow::~MainWindow()
{
    cap.release();
    delete ui;
}

void MainWindow::updateCamera()
{
    Mat frame;
    cap >> frame;

    if(frame.empty()) return;

    Mat gray, blurImg, thresh;

    cvtColor(frame, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, blurImg, Size(15,15),0);

    adaptiveThreshold(
        blurImg,
        thresh,
        255,
        ADAPTIVE_THRESH_GAUSSIAN_C,
        THRESH_BINARY_INV,
        11,
        2
        );

    Mat kernel = getStructuringElement(MORPH_RECT, Size(3,3));
    Mat closing;

    morphologyEx(thresh, closing, MORPH_CLOSE, kernel, Point(-1,-1), 3);

    vector<vector<Point>> contours;

    findContours(closing, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    int objects = 0;

    for(int i=0;i<contours.size();i++)
    {
        vector<Point> cnt = contours[i];

        if(contourArea(cnt) < 3000) continue;

        objects++;

        RotatedRect rect = minAreaRect(cnt);

        Point2f box[4];
        rect.points(box);

        // วาดกรอบเขียว
        for(int j=0;j<4;j++)
        {
            line(frame, box[j], box[(j+1)%4], Scalar(0,255,0),2);
        }

        // คำนวณขนาด
        width_cm = rect.size.width / 25.5;
        height_cm = rect.size.height / 25.5;

        weight =
            (0.0019 * ((2 * width_cm * height_cm * height_cm * 3.14)/3))
            + 0.2228;

        this->width_cm = width_cm;
        this->height_cm = height_cm;
        this->weight = weight;

        // midpoint
        Point2f midTop((box[0].x+box[1].x)/2,(box[0].y+box[1].y)/2);
        Point2f midBottom((box[2].x+box[3].x)/2,(box[2].y+box[3].y)/2);

        Point2f midLeft((box[1].x+box[2].x)/2,(box[1].y+box[2].y)/2);
        Point2f midRight((box[3].x+box[0].x)/2,(box[3].y+box[0].y)/2);

        line(frame, midTop, midBottom, Scalar(255,0,255),2);
        line(frame, midLeft, midRight, Scalar(255,0,255),2);

        circle(frame, midTop,5,Scalar(0,255,0),-1);
        circle(frame, midBottom,5,Scalar(0,255,0),-1);
        circle(frame, midLeft,5,Scalar(0,255,0),-1);
        circle(frame, midRight,5,Scalar(0,255,0),-1);
    }

    char w_text[50];
    char h_text[50];
    char weight_text[50];

    sprintf(w_text, "W: %.2f cm", width_cm);
    sprintf(h_text, "H: %.2f cm", height_cm);
    sprintf(weight_text, "Weight: %.2f kg", weight);

    // แสดงค่าบนหน้าจอ
    string text = "Save in: " + to_string(countdown);

    putText(frame,
            text,
            Point(10,170),
            FONT_HERSHEY_SIMPLEX,
            0.7,
            Scalar(20,200,255),
            2);

    putText(frame,
            w_text,
            Point(10,30),
            FONT_HERSHEY_SIMPLEX,
            0.7,
            Scalar(0,0,255),
            2);

    putText(frame,
            h_text,
            Point(10,60),
            FONT_HERSHEY_SIMPLEX,
            0.7,
            Scalar(0,0,255),
            2);

    putText(frame,
            weight_text,
            Point(10,100),
            FONT_HERSHEY_SIMPLEX,
            0.9,
            Scalar(255,20,127),
            2);

    putText(frame,
            "Objects: " + to_string(objects),
            Point(10,140),
            FONT_HERSHEY_SIMPLEX,
            0.7,
            Scalar(20,200,255),
            2);

    // แปลงไป Qt
    cvtColor(frame, frame, COLOR_BGR2RGB);

    QImage img(
        frame.data,
        frame.cols,
        frame.rows,
        frame.step,
        QImage::Format_RGB888
        );

    ui->label_camera->setPixmap(QPixmap::fromImage(img));
}



void MainWindow::saveWatermelon()
{
    if(width_cm <= 0 || height_cm <= 0 || weight <= 0)
        return;

    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    ui->tableWidget->setItem(row,0,new QTableWidgetItem(QString::number(watermelon_no)));
    ui->tableWidget->setItem(row,1,new QTableWidgetItem(QString::number(width_cm,'f',2)));
    ui->tableWidget->setItem(row,2,new QTableWidgetItem(QString::number(height_cm,'f',2)));
    ui->tableWidget->setItem(row,3,new QTableWidgetItem(QString::number(weight,'f',2)));

    for(int i=0;i<4;i++)
        ui->tableWidget->item(row,i)->setTextAlignment(Qt::AlignCenter);

    watermelon_no++;
}

void MainWindow::on_pushButton_2_clicked()
{
    ui->tableWidget->setRowCount(0);

    watermelon_count = 0;
}
