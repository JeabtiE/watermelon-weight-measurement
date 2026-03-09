#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h"

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

/* ============================================================
   Constructor
============================================================ */

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

/* ============================================================
   Grade Function
============================================================ */

QString getGrade(double weight)
{
    if(weight > 3.0) return "A";
    else if(weight >= 2.0) return "B";
    else return "C";
}

/* ============================================================
   updateCamera()
============================================================ */

void MainWindow::updateCamera()
{
    Mat frame;

    cap >> frame;

    if(frame.empty()) return;

    GaussianBlur(frame, frame, Size(11,11), 0.1);

    currentFrame = frame.clone();

    Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    ////////////////////////////////////////////////////////
    // HSV segmentation (สีแตงโม)
    ////////////////////////////////////////////////////////

    Mat mask_green, mask_dark, mask_light, mask_total;

    Scalar lower_green(20,20,20);
    Scalar upper_green(110,255,255);

    inRange(hsv, lower_green, upper_green, mask_green);

    Scalar lower_dark(0,0,0);
    Scalar upper_dark(180,255,90);

    inRange(hsv, lower_dark, upper_dark, mask_dark);

    Scalar lower_darkgreen(35,10,10);
    Scalar upper_darkgreen(120,255,120);
    Mat mask_darkgreen;

    inRange(hsv, lower_darkgreen, upper_darkgreen, mask_darkgreen);

    Scalar lower_light(20,20,120);
    Scalar upper_light(90,120,255);

    inRange(hsv, lower_light, upper_light, mask_light);

    Mat mask_highlight;

    Scalar lower_high(0,0,200);
    Scalar upper_high(180,40,255);

    inRange(hsv, lower_high, upper_high, mask_highlight);

    mask_total =
        mask_green |
        mask_dark |
        mask_light |
        mask_darkgreen;

    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(9,9));

    morphologyEx(mask_total, mask_total, MORPH_CLOSE, kernel);
    morphologyEx(mask_total, mask_total, MORPH_OPEN, kernel);

    ////////////////////////////////////////////////////////
    // หา contour
    ////////////////////////////////////////////////////////

    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;

    findContours(mask_total, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    int objects = 0;

    double maxArea = 0;
    int maxIndex = -1;


    for(int i = 0; i < (int)contours.size(); i++)
    {
        double area = contourArea(contours[i]);

        if(area < 8000) continue;

        double perimeter = arcLength(contours[i], true);

        double circularity = 4 * CV_PI * area / (perimeter * perimeter);

        if(circularity < 0.45) continue;

        if(area > maxArea)
        {
            maxArea = area;
            maxIndex = i;
        }
    }

    width_cm = 0;
    height_cm = 0;
    weight = 0;

    ////////////////////////////////////////////////////////
    // วัดขนาดวัตถุ
    ////////////////////////////////////////////////////////

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

        // ===== convert pixel → real world =====

        vector<Point2f> src_pts;
        vector<Point2f> dst_pts;

        // จุดปลายของเส้น width
        src_pts.push_back(x1);
        src_pts.push_back(x2);

        // จุดปลายของเส้น height
        src_pts.push_back(y1);
        src_pts.push_back(y2);

        perspectiveTransform(src_pts, dst_pts, homography);

        // ระยะจริง (mm)

        double width_mm = norm(dst_pts[0] - dst_pts[1]);
        double height_mm = norm(dst_pts[2] - dst_pts[3]);

        double width_cm_new = width_mm / 10.0;
        double height_cm_new = height_mm / 10.0;

        ////////////////////////////////////////////////

        static double w_avg = width_cm_new;
        static double h_avg = height_cm_new;

        w_avg = 0.9 * w_avg + 0.1 * width_cm_new;
        h_avg = 0.9 * h_avg + 0.1 * height_cm_new;

        width_cm = w_avg;
        height_cm = h_avg;

        // ===== Improved Weight Estimation =====

        // คำนวณ weight จาก volume
        weight = (0.0018 * ((2 * width_cm * height_cm * height_cm * 3.14) / 3)) + 0.2228;

        // แก้ scale ให้ตรงกับน้ำหนักจริง
        weight = weight / 10.0;

        // smooth ค่า weight
        static double weight_avg = weight;
        weight_avg = 0.9 * weight_avg + 0.1 * weight;
        weight = weight_avg;

        saved = false;
    }


    ////////////////////////////////////////////////////////
    // แสดงผลบนหน้าจอ
    ////////////////////////////////////////////////////////

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

    ////////////////////////////////////////////////////////
    // แสดงภาพใน Qt
    ////////////////////////////////////////////////////////

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

/* ============================================================
   saveWatermelon()
============================================================ */

void MainWindow::saveWatermelon()
{
    if(saved) return;

    if(width_cm <= 0 || height_cm <= 0 || weight <= 0) return;

    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    ui->tableWidget->setItem(row,0,new QTableWidgetItem(QString::number(watermelon_no)));
    ui->tableWidget->setItem(row,1,new QTableWidgetItem(QString::number(width_cm,'f',2)));
    ui->tableWidget->setItem(row,2,new QTableWidgetItem(QString::number(height_cm,'f',2)));
    ui->tableWidget->setItem(row,3,new QTableWidgetItem(QString::number(weight,'f',2)));

    QString grade = getGrade(weight);

    weights.push_back(weight);
    grades.push_back(grade);

    if(grade == "A") countA++;
    else if(grade == "B") countB++;
    else if(grade == "C") countC++;

    ui->tableWidget->setItem(row,4,new QTableWidgetItem(grade));

    for(int i=0;i<4;i++)
        ui->tableWidget->item(row,i)->setTextAlignment(Qt::AlignCenter);

    watermelon_no++;

    QSqlQuery query;

    query.prepare("INSERT INTO watermelon (width,height,weight,grade) VALUES (?,?,?,?)");

    query.addBindValue(width_cm);
    query.addBindValue(height_cm);
    query.addBindValue(weight);
    query.addBindValue(grade);

    query.exec();

    watermelon_images.push_back(currentFrame.clone());

    saved = true;
}

/* ============================================================
   Clear Table
============================================================ */

void MainWindow::on_pushButton_2_clicked()
{
    ui->tableWidget->setRowCount(0);
    watermelon_count = 0;
}

/* ============================================================
   Export CSV
============================================================ */

void MainWindow::on_pushButton_export_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Report",
        "",
        "CSV Files (*.csv)"
        );

    if(fileName.isEmpty()) return;

    QFile file(fileName);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);

    int rowCount = ui->tableWidget->rowCount();
    int colCount = ui->tableWidget->columnCount();

    for(int col=0; col<colCount; col++)
    {
        out << ui->tableWidget->horizontalHeaderItem(col)->text();

        if(col < colCount-1) out << ",";
    }

    out << "\n";

    for(int row=0; row<rowCount; row++)
    {
        for(int col=0; col<colCount; col++)
        {
            QTableWidgetItem *item = ui->tableWidget->item(row,col);

            if(item) out << item->text();

            if(col < colCount-1) out << ",";
        }

        out << "\n";
    }

    file.close();

    QMessageBox::information(this,"Success","Export Complete!");
}

/* ============================================================
   Finish / Dashboard
============================================================ */

void MainWindow::on_pushButton_finish_clicked()
{
    cameraTimer->stop();
    countdownTimer->stop();
    saveTimer->stop();

    int total = weights.size();

    if(total == 0) return;

    double sum = 0;

    for(double w : weights)
        sum += w;

    double avgWeight = sum / total;

    int gradeScore = 0;

    for(const QString &g : grades)
    {
        if(g == "A") gradeScore += 3;
        if(g == "B") gradeScore += 2;
        if(g == "C") gradeScore += 1;
    }

    double avgGrade = (double)gradeScore / total;

    QString text;

    text += "===== DASHBOARD =====\n\n";
    text += "Total Watermelons : " + QString::number(total) + "\n";
    text += "Average Weight : " + QString::number(avgWeight,'f',2) + " kg\n";
    text += "Grade A : " + QString::number(countA) + "\n";
    text += "Grade B : " + QString::number(countB) + "\n";
    text += "Grade C : " + QString::number(countC) + "\n";

    QMessageBox::information(this,"Dashboard",text);
}

/* ============================================================
   Continue
============================================================ */

void MainWindow::on_pushButton_continue_clicked()
{
    cameraTimer->start(30);
    countdownTimer->start(1000);
}
