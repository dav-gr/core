#ifndef CORE_MAIN_WINDOW_H
#define CORE_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <memory>
#include <optional>

#include "core/db_service.h"
#include "core/types.h"

namespace core {
/**
 * @brief Main application window
 * 
 * State model:
 * - connected_: Database connection active
 * - loggedIn_: User authenticated
 * 
 * Tab access:
 * - Import/Export: Requires connected_ && loggedIn_
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

protected slots:
    void onLogin();
    void onLogout();
    void onConfig();
    void onAbout();
    void onConnectionLost();
    void onConnectionRestored();
    void onRefreshStats();

protected:
    void setupUi();
    void setupMenus();
    void loadConfig();
    void saveConfig();
    void tryAutoConnect();
    virtual void updateState();
    void updateStatsDisplay();

    // Database service
    std::unique_ptr<DbService> db_;
    
    // UI components
    QAction* loginAction_ = nullptr;
    QAction* logoutAction_ = nullptr;
    QAction* refreshAction_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    
    // State
    bool connected_ = false;
    bool loggedIn_ = false;
    std::optional<AuthenticatedUser> currentUser_;
    
    // Config
    AppConfig config_;
    QString configPath_;
};

} // namespace core

#endif // CORE_MAIN_WINDOW_H
