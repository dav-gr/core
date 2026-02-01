#ifndef PASSWORD_UTIL_H
#define PASSWORD_UTIL_H

#include <QString>
#include <QCryptographicHash>

namespace core {

/**
 * @brief Password/PIN hashing utility
 * 
 * Uses SHA256 to hash PINs before database operations.
 * The database stores only hashes, never plaintext.
 */
class PasswordUtil {
public:
    /**
     * @brief Hash a PIN using SHA256
     * @param pin The plaintext PIN
     * @return 64-character lowercase hex string
     */
    static QString hashPin(const QString& pin) {
        QByteArray hash = QCryptographicHash::hash(
            pin.toUtf8(),
            QCryptographicHash::Sha256
        );
        return QString(hash.toHex()).toLower();
    }
    
    /**
     * @brief Verify a PIN against a stored hash
     * @param pin The plaintext PIN to verify
     * @param hash The stored hash to compare against
     * @return true if PIN matches hash
     */
    static bool verifyPin(const QString& pin, const QString& hash) {
        return hashPin(pin).compare(hash, Qt::CaseInsensitive) == 0;
    }
};

} // namespace core

#endif // PASSWORD_UTIL_H
