#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include "tcptestserver.h"
#include "tcptestclient.h"

/**
 * @brief 主窗口类，用于选择客户端或服务端模式
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void switchToServer();
    void switchToClient();
    void backToMain();

private:
    void setupUi();

    QWidget *m_mainPage;
    QStackedWidget *m_stackedWidget;
    
    QPushButton *m_serverButton;
    QPushButton *m_clientButton;
    
    TcpTestServer *m_serverWidget;
    TcpTestClient *m_clientWidget;
}; 