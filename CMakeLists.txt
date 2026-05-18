cmake_minimum_required(VERSION 3.16)
project(TSBoss VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport)

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/driverrecord.h
    src/tscalculator.h
    src/tscalculator.cpp
    src/driverdb.h
    src/driverdb.cpp
    src/tswizard.h
    src/tswizard.cpp
    src/quickentrywidget.h
    src/quickentrywidget.cpp
    src/driverlistwidget.h
    src/driverlistwidget.cpp
    src/driverdetailwidget.h
    src/driverdetailwidget.cpp
    src/datasheetentrywidget.h
    src/datasheetentrywidget.cpp
    src/enclosurewidget.h
    src/enclosurewidget.cpp
    src/pdfreport.h
    src/pdfreport.cpp
    src/theme.h
    src/theme.cpp
)

qt_add_executable(TSBoss
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(TSBoss PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::PrintSupport
)

target_include_directories(TSBoss PRIVATE src)

set_target_properties(TSBoss PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

qt_finalize_executable(TSBoss)
