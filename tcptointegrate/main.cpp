#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TCP文件传输");
    app.setApplicationVersion("1.0");

    // 命令行解析
    QCommandLineParser parser;
    parser.setApplicationDescription("TCP文件传输服务端和客户端");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(app);

    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
