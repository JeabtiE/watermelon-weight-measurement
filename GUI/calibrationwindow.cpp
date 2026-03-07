#include "calibrationwindow.h"
#include "ui_calibrationwindow.h"
#include "config.h"
#include "mainwindow.h"

#include <opencv2/opencv.hpp>
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
    delete ui;
}

void CalibrationWindow::updateCamera()
{
    cv::Mat frame;
    cap >> frame;

    if(frame.empty()) return;

    currentFrame = frame.clone();

    cv::Mat gray, edge;

    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    cv::GaussianBlur(gray, gray, cv::Size(5,5),0);

    //Canny edge
    cv::Canny(gray, edge, 30,100);

    std::vector<std::vector<cv::Point>> contours;

    cv::findContours(edge, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    cv::drawContours(frame, contours, -1, cv::Scalar(255,0,0),1);

    qDebug() << "Contours:" << contours.size();

    cv::morphologyEx(edge, edge, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, {5,5}));

    ///////////////////////////////////////////////////////////

    for(auto &cnt : contours)
    {
        double area = cv::contourArea(cnt);

        // contour
        if(area < 500) continue;

        std::vector<cv::Point> approx;

        cv::approxPolyDP(cnt, approx, 0.02 * cv::arcLength(cnt,true), true);

        if(approx.size() == 4)
        {
            cv::RotatedRect rect = cv::minAreaRect(cnt);

            float w = rect.size.width;
            float h = rect.size.height;

            float min_side = std::min(w,h);

            if(min_side == 0) return;

            float ratio = std::max(w,h) / min_side;

            if(ratio > 1.2 && ratio < 2.0) // สัดส่วนบัตร
            {

                qDebug() << "Card detected";

                cv::Point2f box[4];
                rect.points(box);

                for(int i=0;i<4;i++)
                {
                    cv::line(frame, box[i], box[(i+1)%4], cv::Scalar(0,255,0),3);
                }

                double pixel_width = std::max(w,h);
                double real_width = 8.56;

                double new_scale = pixel_width / real_width;

                if(!calibrated)
                {
                    if(pixel_per_cm == 0)
                        pixel_per_cm = new_scale;
                    else
                        pixel_per_cm = 0.9 * pixel_per_cm + 0.1 * new_scale;
                }

                ui->label_scale->setText(QString::number(pixel_per_cm,'f',2));

                break;
            }
        }
    }

    if(pixel_per_cm <= 0)
    {
        putText(frame,
                "Place card for calibration",
                Point(30,50),
                FONT_HERSHEY_SIMPLEX,
                0.7,
                Scalar(0,0,255),
                2);
    }
    else
    {
        putText(frame,
                "Calibration Complete",
                Point(30,50),
                FONT_HERSHEY_SIMPLEX,
                0.7,
                Scalar(0,255,0),
                2);
    }

    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

    QImage img(frame.data,
               frame.cols,
               frame.rows,
               frame.step,
               QImage::Format_RGB888);

    ui->label_camera->setPixmap(QPixmap::fromImage(img));
}

void CalibrationWindow::on_btn_capture_clicked()
{
    calibrated = true;

    qDebug() << "Scale locked:" << pixel_per_cm;

    ui->btn_continue->setEnabled(true);
}

void CalibrationWindow::on_btn_continue_clicked()
{

    if(pixel_per_cm <= 0)
    {
        QMessageBox::warning(this,
                             "Calibration Required",
                             "Please calibrate using the card first!");
        return;
    }

    MainWindow *w = new MainWindow(pixel_per_cm);
    w->show();

    this->close();
}
