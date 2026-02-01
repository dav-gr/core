#include "login_dialog.h"
#include "../utils/password_util.h"
#include "../db/db_service.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

namespace core {

LoginDialog::LoginDialog(DbService* db, QWidget* parent)
    : QDialog(parent)
    , db_(db)
{
    setWindowTitle("Login");
    setModal(true);
    setFixedSize(350, 280);
    
    setupUi();
}

void LoginDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // Login group
    auto* loginGroup = new QGroupBox("Enter Credentials");
    auto* formLayout = new QFormLayout(loginGroup);
    
    usernameEdit_ = new QLineEdit();
    usernameEdit_->setPlaceholderText("Enter your username...");
    usernameEdit_->setMaxLength(100);
    formLayout->addRow("Username:", usernameEdit_);
    
    pinEdit_ = new QLineEdit();
    pinEdit_->setEchoMode(QLineEdit::Password);
    pinEdit_->setPlaceholderText("Enter your PIN...");
    pinEdit_->setMaxLength(20);
    formLayout->addRow("PIN:", pinEdit_);
    
    mainLayout->addWidget(loginGroup);
    
    // Status label
    statusLabel_ = new QLabel();
    statusLabel_->setStyleSheet("color: red;");
    statusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel_);
    
    mainLayout->addStretch();
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    loginButton_ = new QPushButton("Login");
    loginButton_->setDefault(true);
    loginButton_->setEnabled(false);
    buttonLayout->addWidget(loginButton_);
    
    cancelButton_ = new QPushButton("Cancel");
    buttonLayout->addWidget(cancelButton_);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(usernameEdit_, &QLineEdit::textChanged, this, &LoginDialog::onInputChanged);
    connect(pinEdit_, &QLineEdit::textChanged, this, &LoginDialog::onInputChanged);
    connect(pinEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(loginButton_, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
    
    // Focus on username input
    usernameEdit_->setFocus();
}

void LoginDialog::onInputChanged(const QString& ) {
    QString username = usernameEdit_->text().trimmed();
    QString pin = pinEdit_->text().trimmed();
    loginButton_->setEnabled(!username.isEmpty() && !pin.isEmpty());
    statusLabel_->clear();
}

void LoginDialog::onLogin() {
    QString username = usernameEdit_->text().trimmed();
    QString pin = pinEdit_->text().trimmed();
    
    if (username.isEmpty()) {
        statusLabel_->setText("Please enter your username");
        return;
    }
    
    if (pin.isEmpty()) {
        statusLabel_->setText("Please enter your PIN");
        return;
    }
    
    // Disable UI during authentication
    loginButton_->setEnabled(false);
    usernameEdit_->setEnabled(false);
    pinEdit_->setEnabled(false);
    statusLabel_->setText("Authenticating...");
    statusLabel_->setStyleSheet("color: blue;");
    
    // Hash PIN and authenticate
    QString pinHash = PasswordUtil::hashPin(pin);
    
    auto result = db_->authenticate(username, pinHash);
    
    if (result) {
        authUser_ = result;
        accept();
    } else {
        statusLabel_->setText("Invalid username or PIN. Please try again.");
        statusLabel_->setStyleSheet("color: red;");
        usernameEdit_->clear();
        pinEdit_->clear();
        usernameEdit_->setEnabled(true);
        pinEdit_->setEnabled(true);
        usernameEdit_->setFocus();
        loginButton_->setEnabled(false);
    }
}

} // namespace core

