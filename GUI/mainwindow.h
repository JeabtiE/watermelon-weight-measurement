#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <opencv2/opencv.hpp>
#include <QTimer>
#include <QPixmap>
#include <vector>

using namespace cv;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void updateCamera();

    void saveWatermelon();

    void on_pushButton_2_clicked();

    void on_pushButton_export_clicked();

    void on_pushButton_finish_clicked();

    void on_pushButton_continue_clicked();

private:

    Ui::MainWindow *ui;

    cv::VideoCapture cap;
    cv::Mat currentFrame;

    QTimer *cameraTimer;
    QTimer *saveTimer;
    QTimer *countdownTimer;

    float width_cm = 0;
    float height_cm = 0;
    float weight = 0;

    int watermelon_no = 1;
    int watermelon_count = 0;

    int countdown = 10;

    std::vector<double> weights;
    std::vector<QString> grades;
    std::vector<cv::Mat> watermelon_images;

    int countA = 0;
    int countB = 0;
    int countC = 0;

};

#endif // MAINWINDOW_H
