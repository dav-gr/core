#ifndef CORE_DB_SERVICE_ASYNC_WRAPPER_H
#define CORE_DB_SERVICE_ASYNC_WRAPPER_H

#include "db_service.h"

#include <QObject>
#include <QThread>
#include <QString>
#include <QVector>

#include <optional>

namespace core {

class DbServiceWorker : public QObject {
    Q_OBJECT

public:
    explicit DbServiceWorker(QObject* parent = nullptr);

	DbService* getDbService() { return &db_; }

    // sync worker-side helpers
    bool connectToDB(const AppConfig& config);
    std::optional<AuthenticatedUser> authenticate(QString username, QString pinHash);
    QVector<User> getUsers();
    void disconnectDB();
    QVector<Product> getProducts();
    QVector<ProductPackaging> getProductPackaging();
    QVector<PackagePallet> getPackagePallets();
    QString lastError();

public slots:
    void getItemsByStatus(ItemStatus status, ProductionLineId lineId, int limit);
    void assignItemsToBox(const QVector<ItemId>& itemIds, BoxId boxId);
    void updateItemStatus(ItemId itemId, ItemStatus status);
    void addBoxToPallet(BoxId boxId, PalletId palletId);
    void getItems(core::ItemId startId, const QString& gtin, core::ItemStatus status, core::ProductionLineId lineId, int limit);
    void getBoxes(core::BoxId startId, const QString& gtin, core::BoxStatus status, core::ProductionLineId lineId, int limit);
    void getPallets(core::PalletId startId, const QString& gtin, core::PalletStatus status, core::ProductionLineId lineId, int limit);

signals:
    void connected(bool ok, const QString& error);
    void itemsByStatusReady(const QVector<Item>& items, const QString& error);
    void itemsAssignedToBox(int count, const QString& error);
    void itemStatusUpdated(ItemId itemId, bool ok, const QString& error);
    void boxAddedToPallet(BoxId boxId, PalletId palletId, bool ok, const QString& error);
    void itemsReady(const QVector<core::Item>& items, core::ItemStatus status, const QString& error);
    void boxesReady(const QVector<core::Box>& boxes, core::BoxStatus status, const QString& error);
    void palletsReady(const QVector<core::Pallet>& pallets, core::PalletStatus status, const QString& error);

private:
    DbService db_;
};

class DbServiceAsyncWrapper : public QObject {
    Q_OBJECT

public:
    explicit DbServiceAsyncWrapper(QObject* parent = nullptr);
    ~DbServiceAsyncWrapper() override;

    // sync facade
    bool connectToDB(const AppConfig& config);    
    void disconnectDB();
	bool isConnected() const { return m_connected; }
    std::optional<AuthenticatedUser> authenticate(QString username, QString pinHash);
    QVector<User> getUsers();

    QVector<Product> getProducts();
    QVector<ProductPackaging> getProductPackaging();
    QVector<PackagePallet> getPackagePallets();
    QString lastError();

    // async facade
    void connectAsync(const AppConfig& config);
    void getItemsByStatusAsync(ItemStatus status, ProductionLineId lineId, int limit);
    void assignItemsToBoxAsync(const QVector<ItemId>& itemIds, BoxId boxId);
    void updateItemStatusAsync(ItemId itemId, ItemStatus status);
    void addBoxToPalletAsync(BoxId boxId, PalletId palletId);
    void getItemsAsync(core::ItemId startId, const QString& gtin, core::ItemStatus status, core::ProductionLineId lineId, int limit);
    void getBoxesAsync(core::BoxId startId, const QString& gtin, core::BoxStatus status, core::ProductionLineId lineId, int limit);
    void getPalletsAsync(core::PalletId startId, const QString& gtin, core::PalletStatus status, core::ProductionLineId lineId, int limit);

signals:
    void requestConnect(const AppConfig& config);
    void requestGetItemsByStatus(ItemStatus status, ProductionLineId lineId, int limit);
    void requestAssignItemsToBox(const QVector<ItemId>& itemIds, BoxId boxId);
    void requestUpdateItemStatus(ItemId itemId, ItemStatus status);
    void requestAddBoxToPallet(BoxId boxId, PalletId palletId);
    void requestGetItems(core::ItemId startId, const QString& gtin, core::ItemStatus status, core::ProductionLineId lineId, int limit);
    void requestGetBoxes(core::BoxId startId, const QString& gtin, core::BoxStatus status, core::ProductionLineId lineId, int limit);
    void requestGetPallets(core::PalletId startId, const QString& gtin, core::PalletStatus status, core::ProductionLineId lineId, int limit);

    void connected(bool ok, const QString& error);
    void itemsByStatusReady(const QVector<Item>& items, const QString& error);
    void itemsAssignedToBox(int count, const QString& error);
    void itemStatusUpdated(ItemId itemId, bool ok, const QString& error);
    void boxAddedToPallet(BoxId boxId, PalletId palletId, bool ok, const QString& error);
    void itemsReady(const QVector<core::Item>& items, core::ItemStatus status, const QString& error);
    void boxesReady(const QVector<core::Box>& boxes, core::BoxStatus status, const QString& error);
    void palletsReady(const QVector<core::Pallet>& pallets, core::PalletStatus status, const QString& error);

private:
    QThread thread_;
    DbServiceWorker* worker_{ nullptr };
    bool m_connected{ false };

};

} // namespace core

#endif // CORE_DB_SERVICE_ASYNC_WRAPPER_H#endif // CORE_DB_SERVICE_ASYNC_WRAPPER_H