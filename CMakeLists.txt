cmake_minimum_required(VERSION 3.16)
project(deepin-herdr LANGUAGES CXX)

# Default version, overridden by debian/rules via -DAPP_VERSION
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "0.2.0")
endif()
message(STATUS "   >>> Version: ${APP_VERSION}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Use QT_DIR hint from debian/rules for explicit Qt version control
if(DEFINED QT_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${QT_DIR}")
endif()

# Auto-detect Qt version, prefer Qt6 over Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
message(STATUS "   >>> Found Qt version: ${QT_VERSION_MAJOR}")

if(NOT DEFINED ENV{QT_SELECT})
    set(ENV{QT_SELECT} "qt${QT_VERSION_MAJOR}")
endif()
message(STATUS "   >>> Build with Qt version: $ENV{QT_SELECT}")

if(QT_VERSION_MAJOR EQUAL 6)
    set(DTK_VERSION_MAJOR 6)
else()
    set(DTK_VERSION_MAJOR "")
endif()
message(STATUS "   >>> Build with DTK: Dtk${DTK_VERSION_MAJOR}")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools)
find_package(Dtk${DTK_VERSION_MAJOR} COMPONENTS Core Widget REQUIRED)

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
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core5Compat)
    list(APPEND QT_EXTRA_LIBS Qt6::Core5Compat)
endif()

target_link_libraries(deepin-herdr PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    ${QT_EXTRA_LIBS}
    Dtk${DTK_VERSION_MAJOR}::Core
    Dtk${DTK_VERSION_MAJOR}::Widget
    terminalwidget${QT_VERSION_MAJOR}
    PkgConfig::ICU
)

install(TARGETS deepin-herdr DESTINATION bin)
install(FILES src/assets/deepin-herdr.desktop DESTINATION share/applications)
install(FILES src/assets/deepin-herdr.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES ${QM_FILES} DESTINATION share/deepin-herdr/translations)
