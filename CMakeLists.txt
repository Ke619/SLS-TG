cmake_minimum_required(VERSION 3.16)
project(SLS-TG VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

qt_add_executable(SLS-TG
    main.cpp
)

target_link_libraries(SLS-TG PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

set_target_properties(SLS-TG PROPERTIES
    WIN32_EXECUTABLE TRUE
)
