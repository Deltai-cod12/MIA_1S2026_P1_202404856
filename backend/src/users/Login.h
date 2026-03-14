#ifndef LOGIN_H
#define LOGIN_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#include "../models/structs.h"
#include "../commands/mount.h"
#include "../session/session.h"
#include "../commands/users_utils.h"

namespace CommandLogin {

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
        tokens.push_back(item);
    return tokens;
}

inline std::string execute(const std::string& user,
                           const std::string& pass,
                           const std::string& id) {

    if (currentSession.active)
        return "Error: ya existe una sesion activa. Haz logout primero";

    if (user.empty() || pass.empty() || id.empty())
        return "Error: login requiere -user -pass -id";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(id, partition))
        return "Error: particion '" + id + "' no esta montada";

    // Abrir en modo lectura solamente
    std::fstream file(partition.path, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partition.start);
    file.read((char*)&sb, sizeof(Superblock));

    // Leer users.txt completo (multi-bloque)
    std::string content = UsersUtils::readUsersFile(file, sb);
    file.close();

    // Parsear y buscar credenciales
    std::stringstream ss(content);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        auto data = split(line, ',');

        // formato: id,U,grupo,usuario,pass
        if (data.size() != 5) continue;
        if (data[1] != "U")   continue;

        int uid = 0;
        try { uid = std::stoi(data[0]); } catch (...) { continue; }

        // Ignorar usuarios eliminados (id == 0)
        if (uid == 0) continue;

        if (data[3] == user && data[4] == pass) {
            currentSession.active = true;
            currentSession.user   = user;
            currentSession.group  = data[2];
            currentSession.id     = id;
            return "Login exitoso";
        }
    }

    return "Error: usuario o contrasena incorrectos";
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string token, user, pass, id;

    while (iss >> token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if      (lower.find("-user=") == 0) user = token.substr(6);
        else if (lower.find("-pass=") == 0) pass = token.substr(6);
        else if (lower.find("-id=")   == 0) id   = token.substr(4);
    }

    if (user.empty() || pass.empty() || id.empty())
        return "Error: login requiere -user -pass -id";

    return execute(user, pass, id);
}

} // namespace CommandLogin

#endif