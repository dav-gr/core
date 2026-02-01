#ifndef CORE_LOGIN_DIALOG_H
#define CORE_LOGIN_DIALOG_H

#include "../db/types.h"

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <optional>

namespace core {

class DbService;

/**
 * @brief User login dialog
 * 
 * Authenticates user by username and PIN code using DbService.
 */
class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(DbService* db, QWidget* parent = nullptr);
    
    std::optional<AuthenticatedUser> authenticatedUser() const { return authUser_; }

private slots:
    void onLogin();
    void onInputChanged(const QString& text);

private:
    void setupUi();

    DbService* db_;
    
    QLineEdit* usernameEdit_ = nullptr;
    QLineEdit* pinEdit_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    
    std::optional<AuthenticatedUser> authUser_;
};

} // namespace core

#endif // CORE_LOGIN_DIALOG_H
