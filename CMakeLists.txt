cmake_minimum_required(VERSION 3.16)
project(SLS-TG VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

add_executable(SLS-TG WIN32 main.cpp)

target_link_libraries(SLS-TG PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)
