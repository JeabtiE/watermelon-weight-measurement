#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <opencv2/opencv.hpp>
#include <QTimer>
#include <QPixmap>

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

    void on_pushButton_2_clicked();

    void saveWatermelon();

private:
    Ui::MainWindow *ui;

    cv::VideoCapture cap;

    QTimer *saveTimer;

    float width_cm = 0;
    float height_cm = 0;
    float weight = 0;

    int watermelon_no = 1;

    int watermelon_count = 0;

    int countdown = 10;
    QTimer *countdownTimer;
};
#endif // MAINWINDOW_H
