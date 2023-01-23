cmake_minimum_required(VERSION 2.8...3.20)

project(xc-gui LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(XC_GUI_VERSION_MAJOR 0)
set(XC_GUI_VERSION_MINOR 9)
set(XC_GUI_VERSION_RELEASE 7)
set(XC_GUI_VERSION "${XC_GUI_VERSION_MAJOR}.${XC_GUI_VERSION_MINOR}.${XC_GUI_VERSION_RELEASE}")

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

find_package(Qt5 COMPONENTS Widgets Network SerialPort Charts Network REQUIRED)
find_package(Threads REQUIRED)
find_package(M REQUIRED)
find_package(AHPXC REQUIRED)
find_package(AHPGT REQUIRED)
find_package(VLBI REQUIRED)
find_package(DSP REQUIRED)
find_package(FFTW3 REQUIRED)
find_package(M REQUIRED)

include_directories(${CMAKE_INCLUDE_PATH})
include_directories(${DSP_INCLUDE_DIR})
include_directories(${VLBI_INCLUDE_DIR})
include_directories(${AHPGT_INCLUDE_DIR})
include_directories(${AHPXC_INCLUDE_DIR})
include_directories(${FFTW3_INCLUDE_DIR})

if(ANDROID)
    add_library(xc-gui SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/baseline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/baseline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/line.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/line.h
        ${CMAKE_CURRENT_SOURCE_DIR}/line.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/graph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graph.h
        ${CMAKE_CURRENT_SOURCE_DIR}/inputs.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/elemental.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elemental.h
        ${CMAKE_CURRENT_SOURCE_DIR}/types.h
        ${CMAKE_CURRENT_SOURCE_DIR}/threads.h
    )
else(ANDROID)
    add_executable(xc-gui
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/baseline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/baseline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/line.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/line.h
        ${CMAKE_CURRENT_SOURCE_DIR}/line.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/graph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graph.h
        ${CMAKE_CURRENT_SOURCE_DIR}/inputs.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/elemental.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elemental.h
        ${CMAKE_CURRENT_SOURCE_DIR}/types.h
        ${CMAKE_CURRENT_SOURCE_DIR}/threads.h
        ${CMAKE_CURRENT_SOURCE_DIR}/resource.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/app.rc
    )
    if(MINGW)
        target_link_libraries(xc-gui PRIVATE comctl32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--subsystem,windows")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--subsystem,windows")
        set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -Wl,--subsystem,windows -static-libstdc++ -static-libgcc")
        set_target_properties(xc-gui PROPERTIES WIN32_EXECUTABLE TRUE)
    endif(MINGW)
endif(ANDROID)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/icon.ico ${CMAKE_CURRENT_BINARY_DIR}/icon.ico COPYONLY )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xc-gui.iss.cmake ${CMAKE_CURRENT_BINARY_DIR}/xc-gui.iss )

target_link_libraries(xc-gui PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${DSP_LIBRARIES} ${VLBI_LIBRARIES} ${AHPGT_LIBRARIES} ${AHPXC_LIBRARIES} ${FFTW3_LIBRARIES} ${M_LIB} Qt5::Widgets Qt5::Charts Qt5::SerialPort Qt5::Network)
install(TARGETS xc-gui RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bsdl DESTINATION ${CMAKE_INSTALL_PREFIX}/share/ahp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/xc-gui.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/xc-gui.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
