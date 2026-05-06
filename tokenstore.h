#ifndef TOKENSTORE_H
#define TOKENSTORE_H

#include <QString>

namespace token_store {

bool SaveToken(const QString& token);
QString LoadToken();
void RemoveToken();

}

#endif // TOKENSTORE_H
