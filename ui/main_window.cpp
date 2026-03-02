#include "main_window.h"
#include "login_dialog.h"
#include "config_dialog.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QDebug>
#include <QScreen>

namespace core {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , db_(std::make_unique<DbService>(this))
{
    configPath_ = QCoreApplication::applicationDirPath() + "/app.ini";
    
    loadConfig();
    setupUi();
    setupMenus();
    
    // Connect DbService signals
    connect(db_.get(), &DbService::connectionLost, 
            this, &MainWindow::onConnectionLost);
    connect(db_.get(), &DbService::connectionRestored,
            this, &MainWindow::onConnectionRestored);
    
    // Try auto-connect if config is valid
    if (config_.validated && config_.isValid()) {
        tryAutoConnect();
    } else {
        updateState();
    }
    
    setWindowTitle("BackOffice Application v2.0");
    
    // Set window size to fit within available screen geometry (excluding taskbar)
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect availableGeometry = screen->availableGeometry();
        int width = qMin(1200, availableGeometry.width() - 50);
        int height = qMin(800, availableGeometry.height() - 50);
        resize(width, height);
        // Center the window on screen
        move(availableGeometry.x() + (availableGeometry.width() - width) / 2,
             availableGeometry.y() + (availableGeometry.height() - height) / 2);
    } else {
        resize(1000, 700);
    }
}

MainWindow::~MainWindow() {
    if (db_->isConnected()) {
        db_->disconnect();
    }
}

void MainWindow::setupUi() {    
    // Status bar with refresh button on the left
    refreshStatsBtn_ = new QPushButton("Refresh Stats");
    refreshStatsBtn_->setEnabled(false);
    statusBar()->addWidget(refreshStatsBtn_);
    connect(refreshStatsBtn_, &QPushButton::clicked, this, &MainWindow::onRefreshStats);
    
    // Stats label on the right
    statsLabel_ = new QLabel("Not connected");
    statusBar()->addPermanentWidget(statsLabel_);
    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenus() {
    // === File Menu ===
    auto* fileMenu = menuBar()->addMenu("&File");
    
    refreshAction_ = fileMenu->addAction("&Refresh Stats", this, &MainWindow::onRefreshStats);
    refreshAction_->setShortcut(QKeySequence::Refresh);
    refreshAction_->setEnabled(false);
    
    fileMenu->addSeparator();
    
    fileMenu->addAction("E&xit", this, &QMainWindow::close, QKeySequence::Quit);
    
    // === Settings Menu ===
    auto* settingsMenu = menuBar()->addMenu("&Settings");

    loginLogoutAction_ = settingsMenu->addAction("&Login...", this, &MainWindow::onLoginLogout);
    loginLogoutAction_->setShortcut(QKeySequence("Ctrl+L"));

    settingsMenu->addSeparator();
    
    settingsMenu->addAction("&Database Configuration...", this, &MainWindow::onConfig);
    
    // === Help Menu ===
    auto* helpMenu = menuBar()->addMenu("&Help");
    
    helpMenu->addAction("&About...", this, &MainWindow::onAbout);
    helpMenu->addAction("About &Qt...", qApp, &QApplication::aboutQt);
    
    // === Toolbar ===
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    
    // Add spacer to push login/logout to the right
    auto* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    toolbar->addAction(loginLogoutAction_);
}

void MainWindow::loadConfig() {
    QSettings settings(configPath_, QSettings::IniFormat);
    
    settings.beginGroup("Database");
    config_.host = settings.value("host", "localhost").toString();
    config_.port = settings.value("port", 5432).toInt();
    config_.database = settings.value("database", "prod_auto_dev").toString();
    config_.user = settings.value("user", "postgres").toString();
    config_.password = settings.value("password", "hamo1985").toString();
    config_.validated = settings.value("validated", false).toBool();
    settings.endGroup();
    
    qDebug() << "Config loaded from" << configPath_;
}

void MainWindow::saveConfig() {
    QSettings settings(configPath_, QSettings::IniFormat);
    
    settings.beginGroup("Database");
    settings.setValue("host", config_.host);
    settings.setValue("port", config_.port);
    settings.setValue("database", config_.database);
    settings.setValue("user", config_.user);
    settings.setValue("password", config_.password);
    settings.setValue("validated", config_.validated);
    settings.endGroup();
    
    settings.sync();
    qDebug() << "Config saved to" << configPath_;
}

void MainWindow::tryAutoConnect() {
    statusBar()->showMessage("Connecting to database...");
    
    if (db_->connect(config_)) {
        connected_ = true;
        qDebug() << "Auto-connect successful";
    } else {
        connected_ = false;
        qDebug() << "Auto-connect failed:" << db_->lastError();
    }
    
    updateState();
}

void MainWindow::updateState() {   
    // Update login/logout action text and state
    if (loggedIn_) {
        loginLogoutAction_->setText("Log&out");
        loginLogoutAction_->setEnabled(true);
    } else if (connected_) {
        loginLogoutAction_->setText("&Login...");
        loginLogoutAction_->setEnabled(true);
    } else {
        loginLogoutAction_->setText("&Login...");
        loginLogoutAction_->setEnabled(false);
    }

    // Menu/toolbar state
    refreshAction_->setEnabled(connected_);
    refreshStatsBtn_->setEnabled(connected_);

    // Status display
    updateStatsDisplay();
}

void MainWindow::updateStatsDisplay() {
    if (loggedIn_ && currentUser_) {
        auto stats = db_->getStats();
        statsLabel_->setText(QString("User: %1 | Items: %2 | Boxes: %3 | Pallets: %4")
            .arg(currentUser_->user.username)
            .arg(stats.totalItems)
            .arg(stats.totalBoxes)
            .arg(stats.totalPallets));
        statusBar()->showMessage(QString("Connected to %1").arg(config_.displayString()));
    } else if (connected_) {
        statsLabel_->setText("Connected - Please login");
        statusBar()->showMessage(QString("Connected to %1 - Not logged in").arg(config_.displayString()));
    } else if (config_.validated) {
        statsLabel_->setText("Disconnected");
        statusBar()->showMessage("Disconnected from database");
    } else {
        statsLabel_->setText("Not configured");
        statusBar()->showMessage("Please configure database connection");
    }
}

void MainWindow::onLoginLogout() {
    if (loggedIn_) {
        // Handle logout
        auto reply = QMessageBox::question(this, "Confirm Logout",
            "Are you sure you want to logout?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            loggedIn_ = false;
            currentUser_.reset();
            updateState();
            statusBar()->showMessage("Logged out", 3000);
        }
    } else {
        // Handle login
        if (!connected_) {
            QMessageBox::warning(this, "Not Connected",
                "Please connect to the database first via Settings > Database Configuration.");
            return;
        }

        LoginDialog dialog(db_.get(), this);

        if (dialog.exec() == QDialog::Accepted) {
            currentUser_ = dialog.authenticatedUser();
            loggedIn_ = true;

            QMessageBox::information(this, "Login Successful",
                QString("Welcome, %1!").arg(currentUser_->user.fullName));

            updateState();
        }
    }
}

void MainWindow::onConfig() {
    if (connected_) {
        auto reply = QMessageBox::question(this, "Configuration",
            "Changing configuration will disconnect from the database.\nContinue?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            return;
        }
        
        db_->disconnect();
        connected_ = false;
        loggedIn_ = false;
        currentUser_.reset();
        updateState();
    }
    
    ConfigDialog dialog(db_.get(), config_, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        config_ = dialog.config();
        connected_ = dialog.isConnected();
        
        if (connected_) {
            config_.validated = true;
        }
        
        saveConfig();
        updateState();
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About BackOffice",
        "<h2>BackOffice Application v2.0</h2>"
        "<p>BackOffice is a simple, user-friendly administration application for managing production and post-production operations. "
        "It helps accountants and administrators import and export production data, define products and packaging, and manage user access - all from one central interface.</p>"
        "<p><b>Key features</b></p>"
        "<ul>"
        "<li>Import and export production data (items, boxes, pallets)</li>"
        "<li>Maintain product master data (GTINs, names, descriptions)</li>"
        "<li>Configure product packaging (e.g., 6x 0.5L packs)</li>"
        "<li>Manage users, roles, and permissions</li>"
        "</ul>"
        "<p><b>Who should use it</b></p>"
        "<ul>"
        "<li>Administrators: full access to configuration and user management</li>"
        "<li>Accountants: import/export and production-data management</li>"
        "<li>Production staff: use designated production apps; BackOffice is for administrative tasks</li>"
        "</ul>"
        "<p><b>Getting started</b></p>"
        "<ol>"
        "<li>Log in with your BackOffice credentials.</li>"
        "<li>Use the Import tab to load production files.</li>"
        "<li>Use Production Data to add products and packaging.</li>"
        "<li>Use Administration to manage users, roles and permissions.</li>"
        "</ol>"
        "<p>Need help? Contact your system administrator or refer to the Admin Guide in the docs folder for step-by-step instructions and troubleshooting.</p>");
}

void MainWindow::onConnectionLost() {
    connected_ = false;
    loggedIn_ = false;
    currentUser_.reset();
    
    QMessageBox::warning(this, "Connection Lost",
        "The database connection was lost.\n"
        "Please reconnect via Settings > Database Configuration.");
    
    updateState();
}

void MainWindow::onConnectionRestored() {
    connected_ = true;
    statusBar()->showMessage("Connection restored", 3000);
    updateState();
}

void MainWindow::onRefreshStats() {
    if (!connected_) return;
    
    updateStatsDisplay();
    
    statusBar()->showMessage("Stats refreshed", 2000);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (connected_) {
        QString msg = loggedIn_ 
            ? "You are logged in. Disconnect and exit?"
            : "You are connected. Disconnect and exit?";
        
        auto reply = QMessageBox::question(this, "Confirm Exit", msg,
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
        
        db_->disconnect();
    }
    
    event->accept();
}

} // namespace core
