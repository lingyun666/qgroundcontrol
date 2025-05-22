#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QIcon>
#include <QStyle>
#include <QScrollArea>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_stackedWidget(new QStackedWidget(this))
    , m_serverWidget(new TcpTestServer(this))
    , m_clientWidget(new TcpTestClient(this))
{
    setupUi();
    
    // 连接返回按钮
    connect(m_serverWidget, &TcpTestServer::backRequested, this, &MainWindow::backToMain);
    connect(m_clientWidget, &TcpTestClient::backRequested, this, &MainWindow::backToMain);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    // 设置窗口基本属性
    setWindowTitle("TCP文件传输系统");
    resize(1000, 700);
    
    // 创建主页面和滚动区域
    m_mainPage = new QWidget(this);
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_mainPage);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_mainPage);
    mainLayout->setContentsMargins(20, 20, 20, 20); // 增加边距
    
    // 标题
    QLabel *titleLabel = new QLabel("TCP文件传输系统", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // 说明文字
    QLabel *descLabel = new QLabel("请选择运行模式", this);
    descLabel->setAlignment(Qt::AlignCenter);
    QFont descFont = descLabel->font();
    descFont.setPointSize(14);
    descLabel->setFont(descFont);
    mainLayout->addWidget(descLabel);
    
    mainLayout->addSpacing(40);
    
    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    
    // 服务器按钮
    m_serverButton = new QPushButton("服务器模式", this);
    m_serverButton->setMinimumSize(200, 100); // 增大按钮尺寸
    QFont buttonFont = m_serverButton->font();
    buttonFont.setPointSize(14); // 增大字体
    m_serverButton->setFont(buttonFont);
    m_serverButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_serverButton->setIconSize(QSize(48, 48)); // 增大图标
    connect(m_serverButton, &QPushButton::clicked, this, &MainWindow::switchToServer);
    
    // 客户端按钮
    m_clientButton = new QPushButton("客户端模式", this);
    m_clientButton->setMinimumSize(200, 100); // 增大按钮尺寸
    m_clientButton->setFont(buttonFont);
    m_clientButton->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
    m_clientButton->setIconSize(QSize(48, 48)); // 增大图标
    connect(m_clientButton, &QPushButton::clicked, this, &MainWindow::switchToClient);
    
    // 横屏模式布局
    QVBoxLayout *serverButtonLayout = new QVBoxLayout;
    QLabel *serverIconLabel = new QLabel(this);
    serverIconLabel->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(64, 64));
    serverIconLabel->setAlignment(Qt::AlignCenter);
    serverButtonLayout->addWidget(serverIconLabel);
    serverButtonLayout->addWidget(m_serverButton);
    
    QVBoxLayout *clientButtonLayout = new QVBoxLayout;
    QLabel *clientIconLabel = new QLabel(this);
    clientIconLabel->setPixmap(style()->standardIcon(QStyle::SP_DriveNetIcon).pixmap(64, 64));
    clientIconLabel->setAlignment(Qt::AlignCenter);
    clientButtonLayout->addWidget(clientIconLabel);
    clientButtonLayout->addWidget(m_clientButton);
    
    buttonLayout->addStretch();
    buttonLayout->addLayout(serverButtonLayout);
    buttonLayout->addSpacing(50);
    buttonLayout->addLayout(clientButtonLayout);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
    
    // 设置堆叠窗口部件
    m_stackedWidget->addWidget(scrollArea);        // 索引0
    m_stackedWidget->addWidget(m_serverWidget);   // 索引1
    m_stackedWidget->addWidget(m_clientWidget);   // 索引2
    
    // 设置中央部件
    setCentralWidget(m_stackedWidget);
    
    // 设置样式表以支持触摸滚动
    scrollArea->setStyleSheet("QScrollBar:vertical { width: 16px; } QScrollBar:horizontal { height: 16px; }");
}

void MainWindow::switchToServer()
{
    m_stackedWidget->setCurrentIndex(1);
}

void MainWindow::switchToClient()
{
    m_stackedWidget->setCurrentIndex(2);
}

void MainWindow::backToMain()
{
    m_stackedWidget->setCurrentIndex(0);
} 