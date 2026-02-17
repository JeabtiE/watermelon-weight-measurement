cmake_minimum_required(VERSION 3.10)

project(compro)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ระบุ path ที่มี OpenCVConfig.cmake
set(OpenCV_DIR "C:/opencv/build")

# หา OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# สร้าง executable
add_executable(compro watermelon.cpp)

# ลิงก์ไลบรารี
target_link_libraries(compro ${OpenCV_LIBS})
kiiiiii