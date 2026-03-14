#ifndef CHGRP_H
#define CHGRP_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#include "../session/session.h"
#include "../commands/mount.h"
#include "../models/structs.h"
#include "../commands/users_utils.h"

namespace CommandChgrp {

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
        tokens.push_back(item);
    return tokens;
}

inline std::string execute(const std::string& user, const std::string& grp) {

    if (!currentSession.active)
        return "Error: no hay sesion activa";

    if (currentSession.user != "root")
        return "Error: solo root puede ejecutar chgrp";

    if (user == "root")
        return "Error: no se puede cambiar el grupo del usuario root";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(currentSession.id, partition))
        return "Error: particion no montada";

    std::fstream file(partition.path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partition.start);
    file.read((char*)&sb, sizeof(Superblock));

    // Leer users.txt completo (multi-bloque)
    std::string content = UsersUtils::readUsersFile(file, sb);

    // — Primera pasada: validar que usuario y grupo existen —
    std::stringstream ss(content);
    std::string line;
    bool userFound  = false;
    bool groupExists = false;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto data = split(line, ',');
        if (data.size() < 3) continue;

        int id = 0;
        try { id = std::stoi(data[0]); } catch (...) { continue; }

        if (data[1] == "G" && data[2] == grp  && id != 0) groupExists = true;
        if (data[1] == "U" && data.size() >= 5 && data[3] == user && id != 0) userFound = true;
    }

    if (!userFound) {
        file.close();
        return "Error: el usuario '" + user + "' no existe o esta eliminado";
    }
    if (!groupExists) {
        file.close();
        return "Error: el grupo '" + grp + "' no existe o esta eliminado";
    }

    // — Segunda pasada: reconstruir contenido con el grupo cambiado —
    std::stringstream ss2(content);
    std::string newContent;

    while (std::getline(ss2, line)) {
        if (line.empty()) { newContent += "\n"; continue; }
        auto data = split(line, ',');

        // formato usuario: id,U,grupo,usuario,pass
        if (data.size() == 5 && data[1] == "U" && data[3] == user && data[0] != "0") {
            newContent += data[0] + ",U," + grp + "," + data[3] + "," + data[4] + "\n";
        } else {
            newContent += line + "\n";
        }
    }

    // Escribir de vuelta (multi-bloque)
    if (!UsersUtils::writeUsersFile(file, sb, partition.start, newContent)) {
        file.close();
        return "Error: no se pudo escribir users.txt";
    }

    file.close();
    return "Grupo del usuario cambiado correctamente";
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string token, user, grp;

    while (iss >> token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if      (lower.find("-user=") == 0) user = token.substr(6);
        else if (lower.find("-grp=")  == 0) grp  = token.substr(5);
    }

    if (user.empty() || grp.empty())
        return "Error: chgrp requiere -user y -grp";

    return execute(user, grp);
}

} // namespace CommandChgrp

#endif