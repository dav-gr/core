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
    resize(1000, 700);
}

MainWindow::~MainWindow() {
    if (db_->isConnected()) {
        db_->disconnect();
    }
}

void MainWindow::setupUi() {    
    // Status bar with stats label
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
    
    loginAction_ = settingsMenu->addAction("&Login...", this, &MainWindow::onLogin);
    loginAction_->setShortcut(QKeySequence("Ctrl+L"));
    
    logoutAction_ = settingsMenu->addAction("Log&out", this, &MainWindow::onLogout);
    logoutAction_->setEnabled(false);
    
    settingsMenu->addSeparator();
    
    settingsMenu->addAction("&Database Configuration...", this, &MainWindow::onConfig);
    
    // === Help Menu ===
    auto* helpMenu = menuBar()->addMenu("&Help");
    
    helpMenu->addAction("&About...", this, &MainWindow::onAbout);
    helpMenu->addAction("About &Qt...", qApp, &QApplication::aboutQt);
    
    // === Toolbar ===
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->addAction(loginAction_);
    toolbar->addAction(logoutAction_);
    toolbar->addSeparator();
    toolbar->addAction(refreshAction_);
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
    // Menu/toolbar state
    loginAction_->setEnabled(connected_ && !loggedIn_);
    logoutAction_->setEnabled(loggedIn_);
    refreshAction_->setEnabled(connected_);
    
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

void MainWindow::onLogin() {
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

void MainWindow::onLogout() {
    auto reply = QMessageBox::question(this, "Confirm Logout",
        "Are you sure you want to logout?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        loggedIn_ = false;
        currentUser_.reset();
        updateState();
        statusBar()->showMessage("Logged out", 3000);
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
        "<p>Simplified architecture using Qt " QT_VERSION_STR "</p>"
        "<p>Features:</p>"
        "<ul>"
        "<li>PostgreSQL database integration</li>"
        "<li>Import/Export of Items, Boxes, Pallets</li>"
        "<li>User authentication with PIN</li>"
        "</ul>"
        "<p>Built with Qt and C++17</p>");
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
