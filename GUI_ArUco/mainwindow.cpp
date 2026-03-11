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


QString getGrade(double weight)
{
    if(weight > 3.0) return "A";
    else if(weight >= 2.0) return "B";
    else return "C";
}


void MainWindow::updateCamera()
{
    // สร้างตัวแปร Mat สำหรับเก็บภาพจากกล้อง
    Mat frame;

    // อ่านภาพจากกล้อง มาเก็บในตัวแปร frame
    cap >> frame;

    // ถ้าเฟรมว่าง แสดงว่ากล้องอ่านภาพไม่ได้ ให้หยุดฟังก์ชันทันที
    if(frame.empty()) return;

    // ทำ Gaussian Blur เพื่อลด noise ในภาพ
    // kernel ขนาด 11x11 และ sigma = 2
    // ช่วยให้การ segmentation สีแม่นยำขึ้น
    GaussianBlur(frame, frame, Size(11,11), 2);

    // clone ภาพต้นฉบับเก็บไว้ใช้ภายหลัง
    // clone คือการ copy memory จริง ๆ
    currentFrame = frame.clone();

    // สร้าง Mat สำหรับเก็บภาพใน color space แบบ HSV
    Mat hsv;

    // แปลงภาพจาก BGR (รูปแบบสีของ OpenCV)
    // ไปเป็น HSV (Hue Saturation Value)
    // เพราะ HSV แยกสีได้ง่ายกว่าสำหรับ segmentation
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // ระบบ HSV segmentation

    // สร้าง mask สำหรับเก็บพื้นที่สีต่าง ๆ
    Mat mask_green, mask_dark, mask_light, mask_total;

    // กำหนดช่วงสีเขียว (สีหลักของแตงโม)
    Scalar lower_green(35,40,40);   // ค่าต่ำสุดของสีเขียว
    Scalar upper_green(90,255,255); // ค่าสูงสุดของสีเขียว

    // สร้าง mask ที่มีค่า 255 ใน pixel ที่อยู่ในช่วงสีเขียว
    // pixel ที่ไม่ใช่จะเป็น 0
    inRange(hsv, lower_green, upper_green, mask_green);

    // กำหนดช่วงสีมืด (เผื่อกรณีแตงโมมีเงา)
    Scalar lower_dark(0,0,0);
    Scalar upper_dark(180,255,90);

    // ตรวจจับพื้นที่ที่มืดในภาพ
    inRange(hsv, lower_dark, upper_dark, mask_dark);

    // กำหนดช่วง shadow เพิ่มเติม
    Scalar lower_shadow(0,0,0);
    Scalar upper_shadow(180,255,70);

    // ตรวจจับเงาที่มืดมาก
    inRange(hsv, lower_shadow, upper_shadow, mask_dark);

    // กำหนดช่วงสีเขียวเข้ม (ผิวแตงโมบางส่วน)
    Scalar lower_darkgreen(35,10,10);
    Scalar upper_darkgreen(120,255,120);

    // mask สำหรับสีเขียวเข้ม
    Mat mask_darkgreen;

    // ตรวจจับ pixel สีเขียวเข้ม
    inRange(hsv, lower_darkgreen, upper_darkgreen, mask_darkgreen);

    // กำหนดช่วงสีเขียวอ่อน (บริเวณที่สะท้อนแสง)
    Scalar lower_light(20,20,120);
    Scalar upper_light(90,120,255);

    // mask สำหรับสีอ่อน
    inRange(hsv, lower_light, upper_light, mask_light);

    // mask สำหรับ highlight
    Mat mask_highlight;

    // ช่วงสีที่มีความสว่างสูงมาก (แสงสะท้อน)
    Scalar lower_high(0,0,200);
    Scalar upper_high(180,40,255);

    // ตรวจจับ highlight
    inRange(hsv, lower_high, upper_high, mask_highlight);

    // รวม mask ทุกอันเข้าด้วยกันด้วย OR operation
    // เพื่อให้ได้พื้นที่ของแตงโมทั้งหมด
    mask_total =
        mask_green |
        mask_dark |
        mask_light |
        mask_darkgreen;

    // สร้าง kernel สำหรับ morphological operation
    // รูปวงรี ขนาด 9x9
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(9,9));

    // MORPH_CLOSE
    // ปิดรูเล็ก ๆ ใน object และเชื่อม pixel ที่ขาดกัน
    morphologyEx(mask_total, mask_total, MORPH_CLOSE, kernel);

    // MORPH_OPEN
    // ลบ noise ขนาดเล็กที่ไม่ใช่วัตถุจริง
    morphologyEx(mask_total, mask_total, MORPH_OPEN, kernel);

    // หา contour

    // vector เก็บ contour ของวัตถุที่ตรวจพบ
    vector<vector<Point>> contours;

    // hierarchy เก็บความสัมพันธ์ของ contour
    vector<Vec4i> hierarchy;

    // หา contour จากภาพ binary mask
    // RETR_EXTERNAL = เอาเฉพาะ contour ชั้นนอก
    // CHAIN_APPROX_SIMPLE = ลดจำนวน point ของ contour
    findContours(mask_total, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // ตัวแปรเก็บจำนวนวัตถุ
    int objects = 0;

    // เก็บพื้นที่มากที่สุด
    double maxArea = 0;

    // index ของ contour ที่ใหญ่ที่สุด
    int maxIndex = -1;

    // loop ตรวจสอบ contour ทุกอัน
    for(int i = 0; i < (int)contours.size(); i++)
    {
        // คำนวณพื้นที่ของ contour
        double area = contourArea(contours[i]);

        // ถ้าพื้นที่เล็กเกินไป (noise) ให้ข้าม
        if(area < 8000) continue;

        // คำนวณความยาวเส้นรอบรูป
        double perimeter = arcLength(contours[i], true);

        // คำนวณ circularity
        // สูตร = 4πA / P²
        // ใช้ตรวจว่าวัตถุมีลักษณะกลมเหมือนแตงโมหรือไม่
        double circularity = 4 * CV_PI * area / (perimeter * perimeter);

        // ถ้าไม่กลมพอให้ข้าม
        if(circularity < 0.45) continue;

        // เลือก contour ที่มีพื้นที่มากที่สุด
        if(area > maxArea)
        {
            maxArea = area;
            maxIndex = i;
        }
    }

    // reset ค่า measurement ทุก frame
    // เพื่อป้องกันค่าค้างจาก frame ก่อนหน้า
    width_cm = 0;
    height_cm = 0;
    weight = 0;

    // วัดขนาดวัตถุ

    if(maxIndex >= 0) //จับ contour ที่ใหญ่ที่สุด
    {
        objects = 1; // จำนวนวัตถุที่ตรวจพบ

        vector<Point> cnt = contours[maxIndex]; // ดึง contour ของแตงโมออกมา

        RotatedRect rect = minAreaRect(cnt);
        // สร้างสี่เหลี่ยมครอบวัตถุแบบหมุนได้
        // ทำให้วัดขนาดได้ถูกต้องแม้แตงโมจะหมุน แกนจะยังเหมือนเดิม

        Point2f box[4]; // array สำหรับเก็บมุมทั้ง 4 ของสี่เหลี่ยม

        // จุดพิกัด (x , y) ที่เก็บค่าแบบ float

        rect.points(box); // ดึงตำแหน่งมุมทั้ง 4 ของสี่เหลี่ยม

        // วาดกรอบสี่เหลี่ยมรอบแตงโม
        for(int j=0;j<4;j++)
        {
            line(frame, box[j], box[(j+1)%4], Scalar(0,255,0),2);
        }

        float w = rect.size.width;  // ความยาวด้านหนึ่งของกล่อง หน่วย pixel
        float h = rect.size.height; // ความยาวอีกด้านหนึ่งของกล่อง หน่วย pixel

        Point2f center = rect.center; // จุดกึ่งกลางของแตงโม

        // ให้ w เป็นแกนยาวเสมอ เพื่อไม่ให้ค่ากว้าง/ยาวสลับเมื่อวัตถุหมุน น้ำหนักจะเปลี่ยน
        if(w < h)
        {
            swap(w, h);
            rect.angle += 90.0; // ปรับมุมให้ตรงกับแกนใหม่
        }

        float angle = rect.angle * CV_PI / 180.0;
        // แปลงมุมจาก degree เป็น radian เพื่อใช้กับ sin/cos

        // vector ทิศทางของแกนหลัก
        Point2f dx(cos(angle), sin(angle));

        // vector ทิศทางของแกนตั้งฉาก
        Point2f dy(-sin(angle), cos(angle));

        // จุดปลายของแกนความยาว (width axis)
        Point2f x1 = center - dx * (w/2);
        Point2f x2 = center + dx * (w/2);

        // จุดปลายของแกนความกว้าง (height axis)
        Point2f y1 = center - dy * (h/2);
        Point2f y2 = center + dy * (h/2);

        // วาดเส้นแกนความยาวของแตงโม เส้นสีชมพู
        line(frame, x1, x2, Scalar(255,0,255), 2);

        // วาดเส้นแกนความกว้างของแตงโม เส้นสีชมพู
        line(frame, y1, y2, Scalar(255,0,255), 2);

        // วาดจุดกึ่งกลาง สีเหลือง
        circle(frame, center, 5, Scalar(0,255,255), -1);

        if(w < h)
            swap(w,h);

        // แปลงจาก Pixel เป็นขนาดจริง ด้วย Homography

        vector<Point2f> src_pts; // จุดในภาพกล้อง
        vector<Point2f> dst_pts; // จุดหลังแปลงเป็นพิกัดโลกจริง

        // จุดปลายของแกนความยาว
        src_pts.push_back(x1);
        src_pts.push_back(x2);

        // จุดปลายของแกนความกว้าง
        src_pts.push_back(y1);
        src_pts.push_back(y2);

        // แปลงพิกัดจากภาพ เป็นขนาดโลกจริง
        perspectiveTransform(src_pts, dst_pts, homography);

        // คำนวณระยะจริง

        // ระยะจริงของความยาวแตงโม
        double width_mm = norm(dst_pts[0] - dst_pts[1]);

        // ระยะจริงของความกว้างแตงโม
        double height_mm = norm(dst_pts[2] - dst_pts[3]);

        // แปลง mm → cm
        double width_cm_new = width_mm / 10.0;
        double height_cm_new = height_mm / 10.0;

        // Smooth ค่าเพื่อให้ค่าที่วัดนิ่งขึ้น

        static double w_avg = width_cm_new;
        static double h_avg = height_cm_new;

        // moving average filter ลด noise ของ measurement
        w_avg = 0.9 * w_avg + 0.1 * width_cm_new;
        h_avg = 0.9 * h_avg + 0.1 * height_cm_new;

        width_cm = w_avg;
        height_cm = h_avg;

        // คำนวณน้ำหนักจาก Volume

        /*
        1. ปริมาตรประมาณจากสูตรทรงรี Volume ≈ (2 * Width * Height² * π) / 3
        2. Weight = Density × Volume
        */

        // คำนวณน้ำหนักโดยประมาณ
        weight = (0.0018 * ((2 * width_cm * height_cm * height_cm * 3.14) / 3)) + 0.2228;

        weight = weight / 10.0;

        // Smooth ค่า Weight เพื่อลดการแกว่งของค่า

        static double weight_avg = weight;

        // moving average filter
        weight_avg = 0.9 * weight_avg + 0.1 * weight;

        weight = weight_avg;

        saved = false; // ระบุว่ายังไม่ได้บันทึกข้อมูล
    }


    // แสดงผลบนหน้าจอ

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

    // แสดงภาพใน Qt

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


void MainWindow::on_pushButton_2_clicked()
{
    ui->tableWidget->setRowCount(0);
    watermelon_count = 0;
}


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


void MainWindow::on_pushButton_continue_clicked()
{
    cameraTimer->start(30);
    countdownTimer->start(1000);
}
