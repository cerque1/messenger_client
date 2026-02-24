#ifndef REQ_UTILS_H
#define REQ_UTILS_H

#include "request.h"

namespace req_utils{

Request MakeRegistrationRequest(QString login,
                                QString password,
                                QString name,
                                QString birthday,
                                QString registration_date,
                                QString last_online){

    Request req;

    req.setPath("registrate");
    req.setValueToBody("login", login);
    req.setValueToBody("password", password);
    req.setValueToBody("name", name);
    req.setValueToBody("birthday", birthday);
    req.setValueToBody("registration_day", registration_date);
    req.setValueToBody("last_online", last_online);

    return req;
}

Request MakeLoginRequest(QString login,
                         QString password){
    Request req;

    req.setPath("login");
    req.setValueToBody("login", login);
    req.setValueToBody("password", password);

    return req;
}

}

#endif // REQ_UTILS_H
