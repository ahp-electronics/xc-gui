cmake_minimum_required(VERSION 3.5)

project(xc-gui LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

find_package(Qt5 COMPONENTS Widgets SerialPort Charts Network REQUIRED)
find_package(M REQUIRED)
find_package(AHPXC REQUIRED)
find_package(FFTW3 REQUIRED)

include_directories(${AHPXC_INCLUDE_DIR})
include_directories(${FFTW3_INCLUDE_DIR})

if(ANDROID)
  add_library(xc-gui SHARED
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    baseline.cpp
    baseline.h
    line.cpp
    line.h
    line.ui
    graph.cpp
    graph.h
    types.h
    threads.h
  )
else()
  add_executable(xc-gui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    baseline.cpp
    baseline.h
    line.cpp
    line.h
    line.ui
    graph.cpp
    graph.h
    resource.qrc
    types.h
    threads.h
  )
endif()

target_link_libraries(xc-gui PRIVATE ${AHPXC_LIBRARIES} ${FFTW3_LIBRARIES} Qt5::Widgets Qt5::Charts Qt5::SerialPort Qt5::Network)
install(TARGETS xc-gui RUNTIME DESTINATION bin)
install(FILES ahp.png DESTINATION /usr/share/icons/xc-gui.png)
install(FILES app.desktop DESTINATION /usr/share/applications/xc-gui.desktop)
