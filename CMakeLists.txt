cmake_minimum_required(VERSION 3.16)
project(deepin-herdr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(DEFINED QT_DESIRED_VERSION)
    message(STATUS "Build with Qt${QT_DESIRED_VERSION}")
else()
    find_package(Qt6 COMPONENTS Core QUIET)
    if(Qt6Core_FOUND)
        set(QT_DESIRED_VERSION 6)
    else()
        find_package(Qt5 COMPONENTS Core QUIET)
        if(Qt5Core_FOUND)
            set(QT_DESIRED_VERSION 5)
        else()
            message(FATAL_ERROR "Neither Qt5 nor Qt6 found")
        endif()
    endif()
endif()

message(STATUS "Using Qt${QT_DESIRED_VERSION}")

find_package(Qt${QT_DESIRED_VERSION} REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools)
find_package(Dtk${QT_DESIRED_VERSION} COMPONENTS Core Widget REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ICU REQUIRED icu-i18n icu-uc IMPORTED_TARGET)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
)

set(HEADERS
    src/mainwindow.h
)

set(TRANSLATION_FILES
    translations/deepin-herdr.ts
    translations/deepin-herdr_zh_CN.ts
)

foreach(ts_file ${TRANSLATION_FILES})
    qt_add_translation(QM_FILES ${ts_file})
endforeach()

add_executable(deepin-herdr ${SOURCES} ${HEADERS} ${QM_FILES})

target_include_directories(deepin-herdr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/terminalwidget
)

if(QT_DESIRED_VERSION MATCHES 6)
    find_package(Qt6 REQUIRED COMPONENTS Core5Compat LinguistTools)
    target_link_libraries(deepin-herdr PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Core5Compat
        Qt6::Network
        Dtk6::Core
        Dtk6::Widget
        terminalwidget6
        PkgConfig::ICU
    )
elseif(QT_DESIRED_VERSION MATCHES 5)
    find_package(Qt5 REQUIRED COMPONENTS LinguistTools)
    target_link_libraries(deepin-herdr PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
        Dtk::Core
        Dtk::Widget
        terminalwidget5
        PkgConfig::ICU
    )
endif()

install(TARGETS deepin-herdr DESTINATION bin)
install(FILES src/assets/deepin-herdr.desktop DESTINATION share/applications)
install(FILES src/assets/deepin-herdr.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES ${QM_FILES} DESTINATION share/deepin-herdr/translations)
