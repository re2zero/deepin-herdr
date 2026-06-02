cmake_minimum_required(VERSION 3.16)
project(deepin-herdr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)
qt_standard_project_setup()

find_package(Dtk6Core REQUIRED)
find_package(Dtk6Widget REQUIRED)

# Terminal widget from deepin-terminal (terminalwidget6 - QTermWidget fork)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TERMINAL_WIDGET REQUIRED terminalwidget6 IMPORTED_TARGET)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
)

set(HEADERS
    src/mainwindow.h
)

add_executable(deepin-herdr ${SOURCES} ${HEADERS})

target_include_directories(deepin-herdr PRIVATE
    ${TERMINAL_WIDGET_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/terminalwidget6-overlay
)

target_link_libraries(deepin-herdr PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Core5Compat
    Dtk6::Core
    Dtk6::Widget
    PkgConfig::TERMINAL_WIDGET
)

install(TARGETS deepin-herdr DESTINATION bin)
install(FILES resources/deepin-herdr.desktop DESTINATION share/applications)
install(FILES resources/deepin-herdr.svg DESTINATION share/icons/hicolor/scalable/apps)
