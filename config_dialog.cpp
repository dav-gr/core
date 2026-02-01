#include "config_dialog.h"
#include "db_service.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QApplication>

namespace core {

ConfigDialog::ConfigDialog(DbService* db, const AppConfig& config, QWidget* parent)
    : QDialog(parent)
    , db_(db)
    , config_(config)
    , connected_(db->isConnected())
{
    setWindowTitle("Database Configuration");
    setModal(true);
    setMinimumWidth(400);
    
    setupUi();
    loadConfig();
    updateButtonStates();
}

void ConfigDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // Connection settings group
    auto* settingsGroup = new QGroupBox("PostgreSQL Connection");
    auto* formLayout = new QFormLayout(settingsGroup);
    
    hostEdit_ = new QLineEdit();
    hostEdit_->setPlaceholderText("localhost");
    formLayout->addRow("Host:", hostEdit_);
    
    portSpin_ = new QSpinBox();
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(5432);
    formLayout->addRow("Port:", portSpin_);
    
    databaseEdit_ = new QLineEdit();
    databaseEdit_->setPlaceholderText("database_name");
    formLayout->addRow("Database:", databaseEdit_);
    
    userEdit_ = new QLineEdit();
    userEdit_->setPlaceholderText("username");
    formLayout->addRow("User:", userEdit_);
    
    passwordEdit_ = new QLineEdit();
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("password");
    formLayout->addRow("Password:", passwordEdit_);
    
    mainLayout->addWidget(settingsGroup);
    
    // Connection actions
    auto* actionLayout = new QHBoxLayout();
    
    testButton_ = new QPushButton("Test Connection");
    actionLayout->addWidget(testButton_);
    
    connectButton_ = new QPushButton("Connect");
    actionLayout->addWidget(connectButton_);
    
    disconnectButton_ = new QPushButton("Disconnect");
    actionLayout->addWidget(disconnectButton_);
    
    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);
    
    // Status label
    statusLabel_ = new QLabel();
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    mainLayout->addWidget(statusLabel_);
    
    mainLayout->addStretch();
    
    // Dialog buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    okButton_ = new QPushButton("OK");
    okButton_->setDefault(true);
    buttonLayout->addWidget(okButton_);
    
    cancelButton_ = new QPushButton("Cancel");
    buttonLayout->addWidget(cancelButton_);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(testButton_, &QPushButton::clicked, this, &ConfigDialog::onTestConnection);
    connect(connectButton_, &QPushButton::clicked, this, &ConfigDialog::onConnect);
    connect(disconnectButton_, &QPushButton::clicked, this, &ConfigDialog::onDisconnect);
    connect(okButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
    
    // Field change tracking
    connect(hostEdit_, &QLineEdit::textChanged, this, &ConfigDialog::onFieldChanged);
    connect(portSpin_, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &ConfigDialog::onFieldChanged);
    connect(databaseEdit_, &QLineEdit::textChanged, this, &ConfigDialog::onFieldChanged);
    connect(userEdit_, &QLineEdit::textChanged, this, &ConfigDialog::onFieldChanged);
    connect(passwordEdit_, &QLineEdit::textChanged, this, &ConfigDialog::onFieldChanged);
}

void ConfigDialog::loadConfig() {
    hostEdit_->setText(config_.host);
    portSpin_->setValue(config_.port);
    databaseEdit_->setText(config_.database);
    userEdit_->setText(config_.user);
    passwordEdit_->setText(config_.password);
    
    if (connected_) {
        statusLabel_->setText("Connected");
        statusLabel_->setStyleSheet("color: green; font-weight: bold;");
    }
}

void ConfigDialog::updateButtonStates() {
    bool hasValidInput = !hostEdit_->text().isEmpty() &&
                         !databaseEdit_->text().isEmpty() &&
                         !userEdit_->text().isEmpty();
    
    testButton_->setEnabled(hasValidInput && !connected_);
    connectButton_->setEnabled(hasValidInput && !connected_);
    disconnectButton_->setEnabled(connected_);
    
    // Disable editing when connected
    hostEdit_->setEnabled(!connected_);
    portSpin_->setEnabled(!connected_);
    databaseEdit_->setEnabled(!connected_);
    userEdit_->setEnabled(!connected_);
    passwordEdit_->setEnabled(!connected_);
}

void ConfigDialog::onFieldChanged() {
    statusLabel_->clear();
    updateButtonStates();
}

void ConfigDialog::onTestConnection() {
    statusLabel_->setText("Testing connection...");
    statusLabel_->setStyleSheet("color: blue;");
    QApplication::processEvents();
    
    AppConfig testConfig;
    testConfig.host = hostEdit_->text();
    testConfig.port = portSpin_->value();
    testConfig.database = databaseEdit_->text();
    testConfig.user = userEdit_->text();
    testConfig.password = passwordEdit_->text();
    
    QString error;
    bool success = DbService::testConnection(testConfig, &error);
    
    if (success) {
        statusLabel_->setText("Connection successful!");
        statusLabel_->setStyleSheet("color: green; font-weight: bold;");
    } else {
        statusLabel_->setText("Connection failed: " + error);
        statusLabel_->setStyleSheet("color: red;");
    }
}

void ConfigDialog::onConnect() {
    statusLabel_->setText("Connecting...");
    statusLabel_->setStyleSheet("color: blue;");
    QApplication::processEvents();
    
    // Update config from fields
    config_.host = hostEdit_->text();
    config_.port = portSpin_->value();
    config_.database = databaseEdit_->text();
    config_.user = userEdit_->text();
    config_.password = passwordEdit_->text();
    
    if (db_->connect(config_)) {
        connected_ = true;
        statusLabel_->setText("Connected successfully!");
        statusLabel_->setStyleSheet("color: green; font-weight: bold;");
    } else {
        connected_ = false;
        statusLabel_->setText("Connection failed: " + db_->lastError());
        statusLabel_->setStyleSheet("color: red;");
    }
    
    updateButtonStates();
}

void ConfigDialog::onDisconnect() {
    db_->disconnect();
    connected_ = false;
    statusLabel_->setText("Disconnected");
    statusLabel_->setStyleSheet("color: gray;");
    updateButtonStates();
}

} // namespace core
