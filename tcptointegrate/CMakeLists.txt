cmake_minimum_required(VERSION 3.16)

project(tcp2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    tcpserver.h
    tcpserver.cpp
    tcpclient.h
    tcpclient.cpp
    tcptestserver.h
    tcptestserver.cpp
    tcptestclient.h
    tcptestclient.cpp
)

# 使用qt_add_executable而不是add_executable，这将帮助Qt识别为一个应用程序
qt_add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
)

# Android特定配置
if(ANDROID)
    set_property(TARGET ${PROJECT_NAME} PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    
    # 添加Android Java源代码目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android/src")
        set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS 
            "${CMAKE_CURRENT_SOURCE_DIR}/android/src")
    endif()
endif()

# 设置应用程序属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.${PROJECT_NAME}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    # 添加Android包源目录
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# Android平台下的特殊处理 - 移除CorePrivate依赖
if(ANDROID)
    # 确保项目被识别为应用程序
    set_property(TARGET ${PROJECT_NAME} PROPERTY _QT_IS_ANDROID_APP TRUE)
endif()

# 安装
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Android特定配置
if(ANDROID)
    include(${CMAKE_CURRENT_SOURCE_DIR}/android/android_deployment.cmake OPTIONAL)
endif()

# 配置打包信息
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "YourCompany")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TCP文件传输工具")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

# 按系统配置打包
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_MODIFY_PATH ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
elseif(UNIX AND NOT ANDROID)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <your.email@example.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
endif()

include(CPack)
