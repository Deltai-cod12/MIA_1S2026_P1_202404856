#ifndef LOGOUT_H
#define LOGOUT_H

#include "../session/session.h"
#include <string>

namespace CommandLogout {

inline std::string execute(){

    if(!currentSession.active){
        return "Error: no hay sesion activa";
    }

    currentSession.active=false;

    currentSession.user="";
    currentSession.group="";
    currentSession.id="";

    return "Sesion cerrada correctamente";
}

}

#endif