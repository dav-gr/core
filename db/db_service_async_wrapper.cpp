#include "db_service_async_wrapper.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QMetaObject>
#include <QThread>

namespace core {

DbServiceWorker::DbServiceWorker(QObject* parent)
    : QObject(parent) {
}

bool DbServiceWorker::connectToDB(const AppConfig& config) {
    const bool ok = db_.connect(config);
    emit connected(ok, ok ? QString() : db_.lastError());
    return ok;
}

void DbServiceWorker::getItemsByStatus(ItemStatus status, ProductionLineId lineId, int limit) {
    const QVector<Item> items = db_.getItemsByStatus(status, lineId, limit);
    emit itemsByStatusReady(items, QString());
}

void DbServiceWorker::assignItemsToBox(const QVector<ItemId>& itemIds, BoxId boxId) {
    const int count = db_.assignItemsToBox(itemIds, boxId);
    if (count == 0 && !itemIds.isEmpty()) {
        emit itemsAssignedToBox(count, db_.lastError());
        return;
    }
    emit itemsAssignedToBox(count, QString());
}

void DbServiceWorker::updateItemStatus(ItemId itemId, ItemStatus status) {
    QSqlDatabase sqlDb = db_.getDatabase();
    QSqlQuery q(sqlDb);
    q.prepare("UPDATE items SET status = :status WHERE id = :id");
    q.bindValue(":status", static_cast<int>(status));
    q.bindValue(":id", itemId);

    const bool ok = q.exec() && q.numRowsAffected() > 0;
    emit itemStatusUpdated(itemId, ok, ok ? QString() : q.lastError().text());
}

void DbServiceWorker::addBoxToPallet(BoxId boxId, PalletId palletId) {
    const bool ok = db_.assignBoxToPallet(boxId, palletId);
    emit boxAddedToPallet(boxId, palletId, ok, ok ? QString() : db_.lastError());
}

void DbServiceWorker::getItems(core::ItemId startId, const QString& gtin, core::ItemStatus status, core::ProductionLineId lineId, int limit)
{
    const QVector<core::Item> items = db_.getItems(startId, gtin.toStdString(), status, lineId, limit);
    emit itemsReady(items, status, QString());
}

void DbServiceWorker::getBoxes(core::BoxId startId, const QString& gtin, core::BoxStatus status, core::ProductionLineId lineId, int limit)
{
    const QVector<core::Box> boxes = db_.getBoxes(startId, gtin.toStdString(), status, lineId, limit);
    emit boxesReady(boxes, status, QString());
}

void DbServiceWorker::getPallets(core::PalletId startId, const QString& gtin, core::PalletStatus status, core::ProductionLineId lineId, int limit)
{
    const QVector<core::Pallet> pallets = db_.getPallets(startId, gtin.toStdString(), status, lineId, limit);
    emit palletsReady(pallets, status, QString());
}

QVector<User> DbServiceWorker::getUsers()
{
    return db_.getUsers();
}

std::optional<AuthenticatedUser> DbServiceWorker::authenticate(QString username, QString pinHash)
{
    return db_.authenticate(username, pinHash);
}

void DbServiceWorker::disconnectDB() {
    db_.disconnect();
}

QVector<Product> DbServiceWorker::getProducts() {
    return db_.getProducts();
}

QVector<ProductPackaging> DbServiceWorker::getProductPackaging() {
    return db_.getProductPackaging();
}

QVector<PackagePallet> DbServiceWorker::getPackagePallets() {
    return db_.getPackagePallets();
}

DbServiceAsyncWrapper::DbServiceAsyncWrapper(QObject* parent)
    : QObject(parent)
    , worker_(new DbServiceWorker())
{
    worker_->moveToThread(&thread_);

    QObject::connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    QObject::connect(this, &DbServiceAsyncWrapper::requestConnect, worker_, &DbServiceWorker::connectToDB, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestGetItemsByStatus, worker_, &DbServiceWorker::getItemsByStatus, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestAssignItemsToBox, worker_, &DbServiceWorker::assignItemsToBox, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestUpdateItemStatus, worker_, &DbServiceWorker::updateItemStatus, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestAddBoxToPallet, worker_, &DbServiceWorker::addBoxToPallet, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestGetItems, worker_, &DbServiceWorker::getItems, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestGetBoxes, worker_, &DbServiceWorker::getBoxes, Qt::QueuedConnection);
    QObject::connect(this, &DbServiceAsyncWrapper::requestGetPallets, worker_, &DbServiceWorker::getPallets, Qt::QueuedConnection);

    QObject::connect(worker_, &DbServiceWorker::connected, this, &DbServiceAsyncWrapper::connected, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::itemsByStatusReady, this, &DbServiceAsyncWrapper::itemsByStatusReady, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::itemsAssignedToBox, this, &DbServiceAsyncWrapper::itemsAssignedToBox, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::itemStatusUpdated, this, &DbServiceAsyncWrapper::itemStatusUpdated, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::boxAddedToPallet, this, &DbServiceAsyncWrapper::boxAddedToPallet, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::itemsReady, this, &DbServiceAsyncWrapper::itemsReady, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::boxesReady, this, &DbServiceAsyncWrapper::boxesReady, Qt::QueuedConnection);
    QObject::connect(worker_, &DbServiceWorker::palletsReady, this, &DbServiceAsyncWrapper::palletsReady, Qt::QueuedConnection);

    thread_.start();
}

DbServiceAsyncWrapper::~DbServiceAsyncWrapper() {
    thread_.quit();
    thread_.wait();
}

// wrapper sync facade
QVector<User> DbServiceAsyncWrapper::getUsers()
{
    if (!worker_) {
        return {};
    }

    if (QThread::currentThread() == worker_->thread()) {
        return worker_->getUsers();
    }

    QVector<User> result;
    QMetaObject::invokeMethod(worker_, [&]() {
        result = worker_->getUsers();
    }, Qt::BlockingQueuedConnection);
    return result;
}

void DbServiceAsyncWrapper::disconnectDB()
{
    if (!worker_) return;

    if (QThread::currentThread() == worker_->thread()) {
        worker_->disconnectDB();
        return;
    }

    QMetaObject::invokeMethod(worker_, [&]() {
        worker_->disconnectDB();
    }, Qt::BlockingQueuedConnection);
}

QVector<Product> DbServiceAsyncWrapper::getProducts() {
    if (!worker_) return {};

    if (QThread::currentThread() == worker_->thread()) {
        return worker_->getProducts();
    }

    QVector<Product> result;
    QMetaObject::invokeMethod(worker_, [&]() {
        result = worker_->getProducts();
    }, Qt::BlockingQueuedConnection);
    return result;
}

QVector<ProductPackaging> DbServiceAsyncWrapper::getProductPackaging() {
    if (!worker_) return {};

    if (QThread::currentThread() == worker_->thread()) {
        return worker_->getProductPackaging();
    }

    QVector<ProductPackaging> result;
    QMetaObject::invokeMethod(worker_, [&]() {
        result = worker_->getProductPackaging();
    }, Qt::BlockingQueuedConnection);
    return result;
}

QVector<PackagePallet> DbServiceAsyncWrapper::getPackagePallets() {
    if (!worker_) return {};

    if (QThread::currentThread() == worker_->thread()) {
        return worker_->getPackagePallets();
    }

    QVector<PackagePallet> result;
    QMetaObject::invokeMethod(worker_, [&]() {
        result = worker_->getPackagePallets();
    }, Qt::BlockingQueuedConnection);
    return result;
}

bool DbServiceAsyncWrapper::connectToDB(const AppConfig& config)
{
    if (!worker_) return false;

    if (QThread::currentThread() == worker_->thread()) {
        m_connected = worker_->connectToDB(config);
        return m_connected;
    }

    bool connected = false;
    QMetaObject::invokeMethod(worker_, [&]() {
        connected = worker_->connectToDB(config);
    }, Qt::BlockingQueuedConnection);
    m_connected = connected;
    return m_connected;
}

std::optional<AuthenticatedUser> DbServiceAsyncWrapper::authenticate(QString username, QString pinHash)
{
    if (!worker_) return std::nullopt;

    if (QThread::currentThread() == worker_->thread()) {
        return worker_->authenticate(username, pinHash);
    }

    std::optional<AuthenticatedUser> result;
    QMetaObject::invokeMethod(worker_, [&]() {
        result = worker_->authenticate(username, pinHash);
    }, Qt::BlockingQueuedConnection);
    return result;
}

void DbServiceAsyncWrapper::connectAsync(const AppConfig& config) { emit requestConnect(config); }
void DbServiceAsyncWrapper::getItemsByStatusAsync(ItemStatus status, ProductionLineId lineId, int limit) { emit requestGetItemsByStatus(status, lineId, limit); }
void DbServiceAsyncWrapper::assignItemsToBoxAsync(const QVector<ItemId>& itemIds, BoxId boxId) { emit requestAssignItemsToBox(itemIds, boxId); }
void DbServiceAsyncWrapper::updateItemStatusAsync(ItemId itemId, ItemStatus status) { emit requestUpdateItemStatus(itemId, status); }
void DbServiceAsyncWrapper::addBoxToPalletAsync(BoxId boxId, PalletId palletId) { emit requestAddBoxToPallet(boxId, palletId); }
void DbServiceAsyncWrapper::getItemsAsync(core::ItemId startId, const QString& gtin, core::ItemStatus status, core::ProductionLineId lineId, int limit) { emit requestGetItems(startId, gtin, status, lineId, limit); }
void DbServiceAsyncWrapper::getBoxesAsync(core::BoxId startId, const QString& gtin, core::BoxStatus status, core::ProductionLineId lineId, int limit) { emit requestGetBoxes(startId, gtin, status, lineId, limit); }
void DbServiceAsyncWrapper::getPalletsAsync(core::PalletId startId, const QString& gtin, core::PalletStatus status, core::ProductionLineId lineId, int limit) { emit requestGetPallets(startId, gtin, status, lineId, limit); }

QString DbServiceWorker::lastError() {
    return db_.lastError();
}

QString DbServiceAsyncWrapper::lastError() {
    if (!worker_) {
        return QString();
    }

    if (QThread::currentThread() == worker_->thread()) {
        return worker_->lastError();
    }

    QString result;
    QMetaObject::invokeMethod(worker_, [&]() {
        result = worker_->lastError();
    }, Qt::BlockingQueuedConnection);
    return result;
}

} // namespace core} // namespace core