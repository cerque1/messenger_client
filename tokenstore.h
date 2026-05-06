#ifndef TOKENSTORE_H
#define TOKENSTORE_H

#include <QString>

namespace token_store {

struct SessionData {
    QString token;
    int user_id = 0;
    QString user_name;
    bool is_valid = false;
};

bool SaveSession(const SessionData& session);
SessionData LoadSession();

bool SaveToken(const QString& token);
QString LoadToken();
void RemoveToken();

}

#endif // TOKENSTORE_H
