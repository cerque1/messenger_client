#pragma once
#ifndef REQ_UTILS_H
#define REQ_UTILS_H

#include "request.h"

namespace req_utils{

Request MakeRegistrationRequest(QString login,
                                QString password,
                                QString name,
                                QString birthday,
                                QString registration_date,
                                QString last_online);

Request MakeLoginRequest(QString login,
                         QString password);

Request MakeResetConnRequest(QString token);

}

#endif // REQ_UTILS_H

