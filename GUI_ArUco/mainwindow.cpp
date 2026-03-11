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

#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QDateTime>

using namespace cv;
using namespace std;


MainWindow::MainWindow(cv::Mat H, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    homography = H.clone();

    qDebug() << "Homography received";

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("watermelon.db");

    if(!db.open())
    {
        qDebug() << "Database error";
    }
    else
    {
        qDebug() << "Database opened";
        qDebug() << "DB Path:" << QDir::currentPath();
    }

    QSqlQuery query;

    query.exec(
        "CREATE TABLE IF NOT EXISTS watermelon ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "width REAL,"
        "height REAL,"
        "weight REAL,"
        "grade TEXT,"
        "timestamp TEXT)"
        );

    cap.open(0, cv::CAP_DSHOW);

    cameraTimer = new QTimer(this);
    connect(cameraTimer, &QTimer::timeout, this, &MainWindow::updateCamera);
    cameraTimer->start(30);

    saveTimer = new QTimer(this);
    connect(saveTimer, &QTimer::timeout, this, &MainWindow::saveWatermelon);

    countdownTimer = new QTimer(this);
    connect(countdownTimer, &QTimer::timeout, this, [=](){

        countdown--;

        if(countdown <= 0)
        {
            saveWatermelon();
            countdown = 15;
        }

    });

    countdownTimer->start(1000);

    ui->tableWidget->setColumnCount(5);

    ui->tableWidget->setHorizontalHeaderLabels({
        "No :","Width (cm)","Height (cm)","Weight (kg)","Grade"
    });

    ui->tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

MainWindow::~MainWindow()
{
    cap.release();
    delete ui;
}


////// Great ////////


void MainWindow::updateCamera()
{
    ////// Ef ////////


    if(maxIndex >= 0)
    {
        objects = 1;

        vector<Point> cnt = contours[maxIndex]; 

        RotatedRect rect = minAreaRect(cnt);

        Point2f box[4]; 


        rect.points(box); 

        for(int j=0;j<4;j++)
        {
            line(frame, box[j], box[(j+1)%4], Scalar(0,255,0),2);
        }

        float w = rect.size.width;  
        float h = rect.size.height; 

        Point2f center = rect.center; 

        if(w < h)
        {
            swap(w, h);
            rect.angle += 90.0;
        }

        float angle = rect.angle * CV_PI / 180.0;

        Point2f dx(cos(angle), sin(angle));

        Point2f dy(-sin(angle), cos(angle));

        Point2f x1 = center - dx * (w/2);
        Point2f x2 = center + dx * (w/2);

        Point2f y1 = center - dy * (h/2);
        Point2f y2 = center + dy * (h/2);

        line(frame, x1, x2, Scalar(255,0,255), 2);

        line(frame, y1, y2, Scalar(255,0,255), 2);

        circle(frame, center, 5, Scalar(0,255,255), -1);

        if(w < h)
            swap(w,h);


        vector<Point2f> src_pts; 
        vector<Point2f> dst_pts; 

        
        src_pts.push_back(x1);
        src_pts.push_back(x2);

        
        src_pts.push_back(y1);
        src_pts.push_back(y2);

        
        perspectiveTransform(src_pts, dst_pts, homography);

        double width_mm = norm(dst_pts[0] - dst_pts[1]);

        double height_mm = norm(dst_pts[2] - dst_pts[3]);

        double width_cm_new = width_mm / 10.0;
        double height_cm_new = height_mm / 10.0;


        static double w_avg = width_cm_new;
        static double h_avg = height_cm_new;

        w_avg = 0.9 * w_avg + 0.1 * width_cm_new;
        h_avg = 0.9 * h_avg + 0.1 * height_cm_new;

        width_cm = w_avg;
        height_cm = h_avg;

        weight = (0.0018 * ((2 * width_cm * height_cm * height_cm * 3.14) / 3)) + 0.2228;

        weight = weight / 10.0;


        static double weight_avg = weight;

         weight_avg = 0.9 * weight_avg + 0.1 * weight;

        weight = weight_avg;

        saved = false; 
    }


    char w_text[50];
    char h_text[50];
    char weight_text[50];

    sprintf(w_text,"W: %.2f cm",width_cm);
    sprintf(h_text,"H: %.2f cm",height_cm);
    sprintf(weight_text,"Weight: %.2f kg",weight);

    string text = "Save in: " + to_string(countdown);

    putText(frame,text,Point(10,170),FONT_HERSHEY_SIMPLEX,0.7,Scalar(20,200,255),2);
    putText(frame,w_text,Point(10,30),FONT_HERSHEY_SIMPLEX,0.7,Scalar(245,73,39),2);
    putText(frame,h_text,Point(10,60),FONT_HERSHEY_SIMPLEX,0.7,Scalar(245,73,39),2);
    putText(frame,weight_text,Point(10,100),FONT_HERSHEY_SIMPLEX,0.9,Scalar(39,245,73),2);

    putText(frame,"Objects: " + to_string(objects),
            Point(10,140),
            FONT_HERSHEY_SIMPLEX,0.7,Scalar(73,39,245),2);


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

/////// Great /////////

///// Oat ////////

