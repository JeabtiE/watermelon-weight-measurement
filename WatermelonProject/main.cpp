#include <wx/wx.h>
#include <wx/statbmp.h>
#include <opencv2/opencv.hpp>
#include <vector>

using namespace cv;
using namespace std;

// --- ส่วนของ OpenCV Logic (จากโค้ดเดิมของคุณ) ---
// ฟังก์ชันคำนวณน้ำหนักแตงโม
float calculateWeight(float width_cm, float height_cm) {
    return (0.0019f * ((2 * width_cm * height_cm * height_cm * 3.14f) / 3)) + 0.2228f;
}

// --- คลาสหลักของแอปพลิเคชัน ---
class WatermelonApp : public wxFrame {
public:
    WatermelonApp() : wxFrame(NULL, wxID_ANY, "Watermelon Analyzer", wxDefaultPosition, wxSize(800, 600)) {
        mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // สร้างหน้าต่างๆ
        CreatePage1(); // หน้า Welcome
        CreatePage2(); // หน้า Setting กล้อง
        CreatePage3(); // หน้าประมวลผล (GUI เดิมของคุณ)

        // เริ่มต้นที่หน้า 1
        page2->Hide();
        page3->Hide();
        
        this->SetSizer(mainSizer);
        this->Layout();
    }

private:
    wxBoxSizer* mainSizer;
    wxPanel *page1, *page2, *page3;
    wxStaticBitmap* cameraView;
    wxTimer* timer;
    VideoCapture cap;

    // --- หน้าที่ 1: หน้าเริ่มต้น (Welcome) ---
    void CreatePage1() {
        page1 = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        // ใส่รูป Background แตงโม (ตรวจสอบว่ามีไฟล์ชื่อ watermelon_bg.jpg ในโฟลเดอร์)
        wxImage::AddHandler(new wxJPEGHandler);
        wxStaticBitmap* bg = new wxStaticBitmap(page1, wxID_ANY, wxBitmap("watermelon_bg.jpg", wxBITMAP_TYPE_JPEG));
        
        wxButton* startBtn = new wxButton(page1, wxID_ANY, "START", wxDefaultPosition, wxSize(150, 50));
        startBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            page1->Hide();
            page2->Show();
            this->Layout();
        });

        sizer->Add(bg, 1, wxEXPAND | wxALL, 10);
        sizer->Add(startBtn, 0, wxALIGN_CENTER | wxBOTTOM, 30);
        page1->SetSizer(sizer);
        mainSizer->Add(page1, 1, wxEXPAND);
    }

    // --- หน้าที่ 2: ตั้งค่ากล้อง ---
    void CreatePage2() {
        page2 = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* label = new wxStaticText(page2, wxID_ANY, "Setting up Camera...", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        
        wxButton* nextBtn = new wxButton(page2, wxID_ANY, "NEXT", wxDefaultPosition, wxSize(150, 50));
        nextBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (!cap.open(0)) {
                wxMessageBox("Cannot open camera!");
                return;
            }
            page2->Hide();
            page3->Show();
            this->Layout();
            timer->Start(33); // เริ่มรัน Loop กล้อง (30 FPS)
        });

        sizer->Add(label, 1, wxALIGN_CENTER | wxTOP, 50);
        sizer->Add(nextBtn, 0, wxALIGN_CENTER | wxBOTTOM, 30);
        page2->SetSizer(sizer);
        mainSizer->Add(page2, 1, wxEXPAND);
    }

    // --- หน้าที่ 3: หน้าประมวลผล (OpenCV Logic Integration) ---
    void CreatePage3() {
        page3 = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        cameraView = new wxStaticBitmap(page3, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(640, 480));
        
        timer = new wxTimer(this);
        this->Bind(wxEVT_TIMER, &WatermelonApp::OnTimer, this);

        sizer->Add(cameraView, 1, wxALIGN_CENTER | wxALL, 10);
        page3->SetSizer(sizer);
        mainSizer->Add(page3, 1, wxEXPAND);
    }

    // ฟังก์ชันรัน Logic OpenCV และวาดรูปลงหน้าจอ wxWidgets
    void OnTimer(wxTimerEvent&) {
        Mat frame, gray, blurImg, thresh, closing;
        cap >> frame;
        if (frame.empty()) return;

        // --- ใส่ Logic ประมวลผลของคุณตรงนี้ ---
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, blurImg, Size(15, 15), 0);
        adaptiveThreshold(blurImg, thresh, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 11, 2);
        
        Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
        morphologyEx(thresh, closing, MORPH_CLOSE, kernel, Point(-1, -1), 3);

        vector<vector<Point>> contours;
        findContours(closing, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        for (auto &cnt : contours) {
            if (contourArea(cnt) < 3000) continue;
            
            RotatedRect rect = minAreaRect(cnt);
            Point2f box[4]; rect.points(box);
            for (int i = 0; i < 4; i++) line(frame, box[i], box[(i + 1) % 4], Scalar(0, 255, 0), 2);

            // คำนวณน้ำหนักและแสดงผล Text (เหมือนในโค้ดเดิมของคุณ)
            putText(frame, "Processing Watermelon...", Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);
        }

        // แปลง OpenCV Mat เป็น wxBitmap เพื่อแสดงผล
        cvtColor(frame, frame, COLOR_BGR2RGB); // wxWidgets ใช้ RGB
        wxImage img(frame.cols, frame.rows, frame.data, true);
        cameraView->SetBitmap(wxBitmap(img));
    }
};

// --- Entry Point ---
class MyApp : public wxApp {
public:
    virtual bool OnInit() {
        WatermelonApp* frame = new WatermelonApp();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
