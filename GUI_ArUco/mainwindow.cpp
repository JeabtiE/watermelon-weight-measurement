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


///////// oat /////////
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





