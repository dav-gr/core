#ifndef CORE_CONFIG_DIALOG_H
#define CORE_CONFIG_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

#include "../db/types.h"

namespace core {

class DbService;

/**
 * @brief Database configuration dialog
 * 
 * Allows user to configure and test database connection.
 */
class ConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigDialog(DbService* db, const AppConfig& config, QWidget* parent = nullptr);
    
    AppConfig config() const { return config_; }
    bool isConnected() const { return connected_; }

private slots:
    void onTestConnection();
    void onConnect();
    void onDisconnect();
    void onFieldChanged();

private:
    void setupUi();
    void loadConfig();
    void updateButtonStates();

    DbService* db_;
    AppConfig config_;
    bool connected_ = false;
    
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QLineEdit* databaseEdit_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    
    QPushButton* testButton_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QPushButton* okButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    
    QLabel* statusLabel_ = nullptr;
};

} // namespace core

#endif // CORE_CONFIG_DIALOG_H
