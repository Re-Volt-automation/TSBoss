cmake_minimum_required(VERSION 3.16)
project(TSBoss VERSION 1.1.0 LANGUAGES CXX)

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
    src/drivercsv.h
    src/drivercsv.cpp
    src/driverdetailwidget.h
    src/driverdetailwidget.cpp
    src/datasheetentrywidget.h
    src/datasheetentrywidget.cpp
    src/boxmodel.h
    src/boxmodelio.h
    src/boxmodelio.cpp
    src/portphysics.h
    src/bandpassphysics.h
    src/enclosurewidget.h
    src/enclosurewidget.cpp
    src/diagrams/diagrampainter.h
    src/diagrams/diagrampainter.cpp
    src/diagrams/diagramview.h
    src/diagrams/diagramview.cpp
    src/diagrams/portdiagram.h
    src/diagrams/portdiagram.cpp
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

# --- Installation (used by AppImage / .deb packaging) ---
include(GNUInstallDirs)

install(TARGETS TSBoss
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/TSBoss.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES packaging/TSBoss.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
)

# --- Tests ---
enable_testing()
add_executable(portphysics_tests tests/portphysics_tests.cpp)
# Qt6::Gui is required because BoxModel has a QColor field (QtGui), pulled in via boxmodel.h.
target_link_libraries(portphysics_tests PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(portphysics_tests PRIVATE src)
add_test(NAME portphysics_tests COMMAND portphysics_tests)

add_executable(drivercsv_tests tests/drivercsv_tests.cpp src/drivercsv.cpp)
target_link_libraries(drivercsv_tests PRIVATE Qt6::Core)
target_include_directories(drivercsv_tests PRIVATE src)
add_test(NAME drivercsv_tests COMMAND drivercsv_tests)

add_executable(driverdb_tests tests/driverdb_tests.cpp src/driverdb.cpp)
target_link_libraries(driverdb_tests PRIVATE Qt6::Core Qt6::Sql)
target_include_directories(driverdb_tests PRIVATE src)
add_test(NAME driverdb_tests COMMAND driverdb_tests)

# Qt6::Gui for QColor in BoxModel.
add_executable(boxmodelio_tests tests/boxmodelio_tests.cpp src/boxmodelio.cpp)
target_link_libraries(boxmodelio_tests PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(boxmodelio_tests PRIVATE src)
add_test(NAME boxmodelio_tests COMMAND boxmodelio_tests)

add_executable(tscalculator_tests tests/tscalculator_tests.cpp src/tscalculator.cpp)
target_link_libraries(tscalculator_tests PRIVATE Qt6::Core)
target_include_directories(tscalculator_tests PRIVATE src)
add_test(NAME tscalculator_tests COMMAND tscalculator_tests)
