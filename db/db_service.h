#ifndef CORE_DB_SERVICE_H
#define CORE_DB_SERVICE_H

#include "types.h"

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFuture>
#include <QMutex>
#include <optional>

namespace core {

class DbService : public QObject {
    Q_OBJECT

public:
    explicit DbService(QObject* parent = nullptr);
    ~DbService() override;

    // Non-copyable
    DbService(const DbService&) = delete;
    DbService& operator=(const DbService&) = delete;

    // =========================================================================
    // Connection Management
    // =========================================================================
    
    bool connect(const AppConfig& config);
    bool connect(const QString& host, int port, const QString& database,
                 const QString& user, const QString& password);
    void disconnect();
    bool isConnected() const;
    bool reconnect();
    QString lastError() const;
    
    // Test connection without storing
    static bool testConnection(const AppConfig& config, QString* errorOut = nullptr);
    static QSqlDatabase createThreadLocalConnection(const QString& host, int port, const QString& database,
                                             const QString& user, const QString& password);

    // =========================================================================
    // Authentication (SYNC)
    // =========================================================================
    
    std::optional<AuthenticatedUser> authenticate(const QString& username, const QString& pinHash);
    std::optional<User> getUserByUsername(const QString& username);
    std::optional<User> getUser(UserId userId);

    // =========================================================================
    // Production Lines (SYNC)
    // =========================================================================
    
    QVector<ProductionLine> getProductionLines();
    std::optional<ProductionLine> getProductionLine(ProductionLineId id);

    // =========================================================================
    // Products (SYNC)
    // =========================================================================

    QVector<Product> getProducts();
    std::optional<Product> getProduct(ProductId id);
    bool createProduct(const Product& product);
    bool updateProduct(const Product& product);
    bool deleteProduct(ProductId id);

    // =========================================================================
    // Product Packaging (SYNC)
    // =========================================================================

    QVector<ProductPackaging> getProductPackaging();
    std::optional<ProductPackaging> getPackaging(ProductPackagingId id);
    bool createPackaging(const ProductPackaging& packaging);
    bool updatePackaging(const ProductPackaging& packaging);
    bool deletePackaging(ProductPackagingId id);

    // =========================================================================
    // User Management (SYNC)
    // =========================================================================

    QVector<User> getUsers();
    bool createUser(const User& user);
    bool updateUser(const User& user);
    bool deleteUser(UserId userId);

    // =========================================================================
    // Role Management (SYNC)
    // =========================================================================

    QVector<Role> getRoles();
    std::optional<Role> getRole(RoleId roleId);
    bool createRole(const Role& role);
    bool updateRole(const Role& role);
    bool deleteRole(RoleId roleId);
    QVector<Role> getUserRoles(UserId userId);
    bool assignRoleToUser(UserId userId, RoleId roleId);
    bool removeRoleFromUser(UserId userId, RoleId roleId);

    // =========================================================================
    // Permission Management (SYNC)
    // =========================================================================

    QVector<Permission> getPermissions();
    QVector<Permission> getRolePermissions(RoleId roleId);
    bool assignPermissionToRole(RoleId roleId, qint32 permissionId);
    bool removePermissionFromRole(RoleId roleId, qint32 permissionId);

    // =========================================================================
    // Import Operations (ASYNC)
    // =========================================================================

    QFuture<ImportResult> importItemsAsync(const QString& filePath, 
                                            ProductionLineId lineId,
                                            UserId userId);
    QFuture<ImportResult> importBoxesAsync(const QString& filePath,
                                            ProductionLineId lineId,
                                            UserId userId);
    QFuture<ImportResult> importPalletsAsync(const QString& filePath,
                                              ProductionLineId lineId,
                                              UserId userId);

    // Import Document Management (SYNC)
    QVector<ImportDocument> getItemsImportDocuments(ProductionLineId lineId = 0, int limit = 50);
    QVector<ImportDocument> getBoxesImportDocuments(ProductionLineId lineId = 0, int limit = 50);
    QVector<ImportDocument> getPalletsImportDocuments(ProductionLineId lineId = 0, int limit = 50);
    bool deleteItemsImportDocument(ImportDocumentId docId);
    bool deleteBoxesImportDocument(ImportDocumentId docId);
    bool deletePalletsImportDocument(ImportDocumentId docId);

    // Import Document Deletion (ASYNC)
    QFuture<bool> deleteItemsImportDocumentAsync(ImportDocumentId docId);
    QFuture<bool> deleteBoxesImportDocumentAsync(ImportDocumentId docId);
    QFuture<bool> deletePalletsImportDocumentAsync(ImportDocumentId docId);

    // =========================================================================
    // Item Operations (SYNC)
    // =========================================================================
    
    std::optional<Item> getItem(ItemId id);
    QVector<Item> getItemsByStatus(ItemStatus status, ProductionLineId lineId, int limit);
    QVector<Item> getItems(ItemId startId, const std::string& gtin, ItemStatus status, ProductionLineId lineId, int limit);
    QVector<Item> getItemsInBox(BoxId boxId);
    int countScannedItemsNotInBox(ProductionLineId lineId);
    bool assignItemToBox(ItemId itemId, BoxId boxId);
    int assignItemsToBox(const QVector<ItemId>& itemIds, BoxId boxId);
    QVector<Item> getScannedItemsNotInBox(ProductionLineId lineId, int limit);

    // =========================================================================
    // Box Operations (SYNC)
    // =========================================================================
    
    std::optional<core::Box> getBox(BoxId id);
    QVector<Box> getBoxesByStatus(BoxStatus status, ProductionLineId lineId, int limit);
    QVector<Box> getBoxes(BoxId startId, const std::string&, BoxStatus status, ProductionLineId lineId, int limit);

    QVector<Box> getSealedBoxesNotOnPallet(ProductionLineId lineId, int limit);
    int countSealedBoxesNotOnPallet(ProductionLineId lineId);
    QVector<Box> getBoxesOnPallet(PalletId palletId);
    bool sealBox(BoxId id);
    bool assignBoxToPallet(BoxId boxId, PalletId palletId);
    int getBoxItemCount(BoxId id);

    // Add in class DbService public section (near createThreadLocalConnection)
    static bool updateItemStatus(QSqlDatabase& db, ItemId itemId, ItemStatus status);
    static bool assignItemsToBox(QSqlDatabase& db, const QVector<ItemId>& itemIds, BoxId boxId);
	static bool addBoxToPallet(QSqlDatabase& db, BoxId boxId, PalletId palletId);

    // =========================================================================
    // Pallet Operations (SYNC)
    // =========================================================================
    
    std::optional<Pallet> getPallet(PalletId id);
    QVector<Pallet> getPalletsByStatus(PalletStatus status, ProductionLineId lineId = 0, int limit = 100);
    QVector<Pallet> getPallets(PalletId startId, const std::string& gtin, PalletStatus status, ProductionLineId lineId, int limit);

    bool completePallet(PalletId id);

    QVector<Pallet> getPallets();
    bool createPallet(const Pallet& pallet);
    bool updatePallet(const Pallet& pallet);
    bool deletePallet(PalletId id);

    // PackagePallet operations (CRUD for package_pallet table)
    bool createPackagePallet(const PackagePallet& packagePallet);
    bool updatePackagePallet(const PackagePallet& packagePallet);
    bool deletePackagePallet(PackagePalletId id);
    QVector<PackagePallet> getPackagePallets(int limit = 100, int offset = 0);

    // =========================================================================
    // Export Operations (ASYNC)
    // =========================================================================
    
    QFuture<ExportResult> exportItemsAsync(const QVector<ItemId>& itemIds,
                                            const QString& lpTin);
    QFuture<ExportResult> exportBoxesAsync(const QVector<BoxId>& boxIds, 
                                            const QString& lpTin);
    QFuture<ExportResult> exportPalletsAsync(const QVector<PalletId>& palletIds,
                                              const QString& lpTin);
    // Export scanned (not in box) items by fetching them from DB

    std::optional<ExportDocument> getExportDocument(ExportDocumentId id);
    QVector<ExportDocument> getExportDocuments(int limit = 50, int offset = 0);
    int getExportDocumentItemCount(ExportDocumentId id);
    int getExportDocumentBoxCount(ExportDocumentId id);
    int getExportDocumentPalletCount(ExportDocumentId id);

    // =========================================================================
    // Statistics (SYNC)
    // =========================================================================
    
    ProductionStats getStats(std::optional<ProductionLineId> lineId = std::nullopt);

    // =========================================================================
    // Pipeline Support Methods (SYNC)
    // =========================================================================

    // Entity Resolver: find entity by barcode in global tables
    // Searches pallets ? boxes ? items
    std::optional<ResolvedEntity> findEntityByBarcode(const QString& barcode);

    // State Resolver: pallet state queries
    int countBoxesOnPallet(PalletId palletId);
    bool isBoxOnPallet(BoxId boxId);
    std::optional<PalletId> findPalletForBox(BoxId boxId);

    // State Resolver: box state queries
    bool isBoxFree(BoxId boxId);

    // State Resolver: item state queries
    std::optional<core::BoxId> findBoxForItem(ItemId itemId);

    // GTIN lookup helpers
    QString getPackagingGtin(ProductPackagingId packagingId);

    // Action Executor: barcode lookup in specific dynamic tables
    std::optional<Box> findBoxByBarcode(const QString& barcode);
    std::optional<Item> findItemByBarcode(const QString& barcode);

    // Action Executor: atomic action operations (use transactions internally)
    ActionResult unsealBoxAction(BoxId boxId);
    ActionResult destroyItemAction(ItemId itemId);


    // =========================================================================
    // Database Access
    // =========================================================================
    
    QSqlDatabase getDatabase();

signals:
    void connectionLost();
    void connectionRestored();
    void importProgress(int current, int total);
    void deleteProgress(int current, int total);

private:
    bool ensureConnected();

    // Import helpers
    ImportResult doImport(const QString& filePath, ProductionLineId lineId,
                          const QString& tableName, const QString& importTableName,
                          UserId userId);

    // Delete helpers
    bool doDeleteImportDocument(ImportDocumentId docId, const QString& tableName,
                                const QString& importTableName, const QString& junctionTableName,
                                const QString& entityIdCol);
   
    // Export helpers
    ExportResult doExportItems(const QVector<ItemId>& itemIds, const QString& lpTin);
    ExportResult doExportBoxes(const QVector<BoxId>& boxIds, const QString& lpTin);
    ExportResult doExportPallets(const QVector<PalletId>& palletIds, const QString& lpTin);
    QString generateItemExportXml(ExportDocumentId docId, const QString& lpTin, QSqlDatabase& db);
    QString generateBoxExportXml(ExportDocumentId docId, const QString& lpTin, QSqlDatabase& db);
    QString generatePalletExportXml(ExportDocumentId docId, const QString& lpTin, QSqlDatabase& db);
    QString cleanBarcodeForExport(const QString& barcode);
    
    // Parse helpers
    User parseUser(const QSqlQuery& query);
    Role parseRole(const QSqlQuery& query);
    Permission parsePermission(const QSqlQuery& query);
    Item parseItem(const QSqlQuery& query);
    Box parseBox(const QSqlQuery& query);
    Pallet parsePallet(const QSqlQuery& query);
    ImportDocument parseImportDocument(const QSqlQuery& query);
    ExportDocument parseExportDocument(const QSqlQuery& query);
    Product parseProduct(const QSqlQuery& query);
    ProductPackaging parseProductPackaging(const QSqlQuery& query);
    PackagePallet parsePackagePallet(const QSqlQuery& query);
    
    QString buildPlaceholders(const QStringList& values);

    QString connectionName_;
    mutable QMutex mutex_;
    QString lastError_;
    AppConfig config_;
};

} // namespace core

#endif // CORE_DB_SERVICE_H
