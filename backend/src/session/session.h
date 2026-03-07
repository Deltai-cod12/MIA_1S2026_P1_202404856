#ifndef SESSION_H
#define SESSION_H

#include <string>

// Estructura que representa una sesión activa
struct Session {

    bool active;         // Indica si hay una sesión activa
    std::string user;    // Usuario logueado
    std::string group;   // Grupo del usuario
    std::string id;      // ID de la partición montada

    Session() {
        active = false;
        user = "";
        group = "";
        id = "";
    }
};

// Variable global de sesión
static Session currentSession;

#endif