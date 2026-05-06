#include "tokenstore.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QCoreApplication>

namespace {
QString TokenFilePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.messenger_client";
    }
    QDir().mkpath(dir);
    return dir + "/session.dat";
}

QByteArray BuildKey() {
    const QString seed = QCoreApplication::applicationName() + "|"
        + QCoreApplication::organizationName() + "|"
        + QDir::homePath();
    return QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
}

QByteArray XorCipher(const QByteArray& input, const QByteArray& key) {
    if (key.isEmpty()) {
        return input;
    }

    QByteArray output = input;
    for (int i = 0; i < output.size(); ++i) {
        output[i] = output[i] ^ key.at(i % key.size());
    }
    return output;
}
}

namespace token_store {

bool SaveToken(const QString& token) {
    QFile file(TokenFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QByteArray encrypted = XorCipher(token.toUtf8(), BuildKey()).toBase64();
    file.write(encrypted);
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

    const QByteArray encrypted = QByteArray::fromBase64(encoded);
    if (encrypted.isEmpty()) {
        return "";
    }

    return QString::fromUtf8(XorCipher(encrypted, BuildKey()));
}

void RemoveToken() {
    QFile::remove(TokenFilePath());
}

}
