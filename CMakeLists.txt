cmake_minimum_required(VERSION 3.16)
project(TSBoss VERSION 1.3.0 LANGUAGES CXX)

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
    src/plots/plotsupport.h
    src/plots/plotbase.h
    src/plots/plotbase.cpp
    src/plots/modelplots.h
    src/plots/modelplots.cpp
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
    packaging/resources.qrc
)

# Windows exe / taskbar icon — compiled into the binary via a resource script.
if(WIN32)
    enable_language(RC)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/packaging")
    list(APPEND PROJECT_SOURCES packaging/app.rc)
endif()

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

add_executable(filters_tests tests/filters_tests.cpp)
target_include_directories(filters_tests PRIVATE src)
add_test(NAME filters_tests COMMAND filters_tests)

add_executable(driverfilter_tests tests/driverfilter_tests.cpp)
target_link_libraries(driverfilter_tests PRIVATE Qt6::Core)
target_include_directories(driverfilter_tests PRIVATE src)
add_test(NAME driverfilter_tests COMMAND driverfilter_tests)

# Qt6::Gui for QColor in BoxModel (modeleval.h is header-only).
add_executable(modeleval_tests tests/modeleval_tests.cpp)
target_link_libraries(modeleval_tests PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(modeleval_tests PRIVATE src)
add_test(NAME modeleval_tests COMMAND modeleval_tests)

# Cabin-gain environment: correction shape, REW parsing, Helmholtz fit.
add_executable(cabingain_tests tests/cabingain_tests.cpp)
target_link_libraries(cabingain_tests PRIVATE Qt6::Core)
target_include_directories(cabingain_tests PRIVATE src)
add_test(NAME cabingain_tests COMMAND cabingain_tests)

# Isobaric mounting transform + vented-equivalence characterization.
add_executable(isobaric_tests tests/isobaric_tests.cpp src/tscalculator.cpp)
target_link_libraries(isobaric_tests PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(isobaric_tests PRIVATE src)
add_test(NAME isobaric_tests COMMAND isobaric_tests)

# Driver-parameter sanity checks (Qt6::Gui for QColor in BoxModel).
add_executable(paramcheck_tests tests/paramcheck_tests.cpp)
target_link_libraries(paramcheck_tests PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(paramcheck_tests PRIVATE src)
add_test(NAME paramcheck_tests COMMAND paramcheck_tests)

# Decimal-separator behaviour of the shared spinbox classes (headless).
add_executable(spinbox_tests tests/spinbox_tests.cpp)
target_link_libraries(spinbox_tests PRIVATE Qt6::Widgets)
target_include_directories(spinbox_tests PRIVATE src)
add_test(NAME spinbox_tests COMMAND spinbox_tests)

# Offscreen render smoke test for the plot widgets. Pulls in the widget
# dependency closure; runs headless via QT_QPA_PLATFORM=offscreen.
add_executable(plotrender_tests
    tests/plotrender_tests.cpp
    src/enclosurewidget.cpp
    src/plots/plotbase.cpp
    src/plots/modelplots.cpp
    src/boxmodelio.cpp
    src/tscalculator.cpp
    src/driverdb.cpp
    src/driverdetailwidget.cpp
    src/theme.cpp
    src/pdfreport.cpp
    src/diagrams/diagrampainter.cpp
    src/diagrams/diagramview.cpp
    src/diagrams/portdiagram.cpp
)
target_link_libraries(plotrender_tests PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport)
target_include_directories(plotrender_tests PRIVATE src)
add_test(NAME plotrender_tests COMMAND plotrender_tests)
