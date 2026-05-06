#include "tokenstore.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QByteArray>

#include <openssl/evp.h>

namespace {
constexpr int kSaltSize = 16;
constexpr int kIvSize = 12;
constexpr int kTagSize = 16;
constexpr int kKeySize = 32;
constexpr int kPbkdf2Iterations = 120000;

QString TokenFilePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.messenger_client";
    }
    QDir().mkpath(dir);
    return dir + "/session.dat";
}

QByteArray BuildSecretSeed() {
    const QString seed = QCoreApplication::applicationName() + "|"
        + QCoreApplication::organizationName() + "|"
        + QDir::homePath();
    return QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha512);
}

QByteArray RandomBytes(int size) {
    QByteArray bytes(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }
    return bytes;
}

bool DeriveKey(const QByteArray& salt, QByteArray* key) {
    if (!key || salt.size() != kSaltSize) {
        return false;
    }

    key->resize(kKeySize);
    const QByteArray secret = BuildSecretSeed();
    const int ok = PKCS5_PBKDF2_HMAC(
        secret.constData(), secret.size(),
        reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
        kPbkdf2Iterations,
        EVP_sha256(),
        kKeySize,
        reinterpret_cast<unsigned char*>(key->data()));

    return ok == 1;
}

QByteArray EncryptAesGcm(const QByteArray& plain, const QByteArray& key, const QByteArray& iv, QByteArray* outTag) {
    if (!outTag || key.size() != kKeySize || iv.size() != kIvSize) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    QByteArray cipher(plain.size(), 0);
    int len = 0;
    int cipherLen = 0;

    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                                 reinterpret_cast<const unsigned char*>(key.constData()),
                                 reinterpret_cast<const unsigned char*>(iv.constData())) == 1;

    if (ok && !plain.isEmpty()) {
        ok = EVP_EncryptUpdate(ctx,
                               reinterpret_cast<unsigned char*>(cipher.data()), &len,
                               reinterpret_cast<const unsigned char*>(plain.constData()), plain.size()) == 1;
        cipherLen = len;
    }

    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx,
                                 reinterpret_cast<unsigned char*>(cipher.data()) + cipherLen,
                                 &len) == 1;
        cipherLen += len;
    }

    outTag->resize(kTagSize);
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagSize, outTag->data()) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        outTag->clear();
        return {};
    }

    cipher.resize(cipherLen);
    return cipher;
}

QByteArray DecryptAesGcm(const QByteArray& cipher, const QByteArray& key, const QByteArray& iv, const QByteArray& tag) {
    if (key.size() != kKeySize || iv.size() != kIvSize || tag.size() != kTagSize) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    QByteArray plain(cipher.size(), 0);
    int len = 0;
    int plainLen = 0;

    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                                 reinterpret_cast<const unsigned char*>(key.constData()),
                                 reinterpret_cast<const unsigned char*>(iv.constData())) == 1;

    if (ok && !cipher.isEmpty()) {
        ok = EVP_DecryptUpdate(ctx,
                               reinterpret_cast<unsigned char*>(plain.data()), &len,
                               reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.size()) == 1;
        plainLen = len;
    }

    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagSize, const_cast<char*>(tag.constData())) == 1;
    }

    if (ok) {
        ok = EVP_DecryptFinal_ex(ctx,
                                 reinterpret_cast<unsigned char*>(plain.data()) + plainLen,
                                 &len) == 1;
        plainLen += len;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        return {};
    }

    plain.resize(plainLen);
    return plain;
}
}

namespace token_store {

bool SaveToken(const QString& token) {
    const QByteArray salt = RandomBytes(kSaltSize);
    const QByteArray iv = RandomBytes(kIvSize);

    QByteArray key;
    if (!DeriveKey(salt, &key)) {
        return false;
    }

    QByteArray tag;
    const QByteArray cipher = EncryptAesGcm(token.toUtf8(), key, iv, &tag);
    if (cipher.isEmpty() && !token.isEmpty()) {
        return false;
    }

    QByteArray payload;
    payload.reserve(4 + salt.size() + iv.size() + tag.size() + cipher.size());
    payload.append("A1::");
    payload.append(salt);
    payload.append(iv);
    payload.append(tag);
    payload.append(cipher);

    QFile file(TokenFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(payload.toBase64());
    file.close();
    return true;
}

QString LoadToken() {
    QFile file(TokenFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return "";
    }

    const QByteArray encoded = file.readAll().trimmed();
    file.close();
    if (encoded.isEmpty()) {
        return "";
    }

    const QByteArray payload = QByteArray::fromBase64(encoded);
    if (!payload.startsWith("A1::") || payload.size() < 4 + kSaltSize + kIvSize + kTagSize) {
        return "";
    }

    const int offset = 4;
    const QByteArray salt = payload.mid(offset, kSaltSize);
    const QByteArray iv = payload.mid(offset + kSaltSize, kIvSize);
    const QByteArray tag = payload.mid(offset + kSaltSize + kIvSize, kTagSize);
    const QByteArray cipher = payload.mid(offset + kSaltSize + kIvSize + kTagSize);

    QByteArray key;
    if (!DeriveKey(salt, &key)) {
        return "";
    }

    const QByteArray plain = DecryptAesGcm(cipher, key, iv, tag);
    return QString::fromUtf8(plain);
}

void RemoveToken() {
    QFile::remove(TokenFilePath());
}

}
