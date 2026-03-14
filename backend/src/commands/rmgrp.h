#ifndef RMGRP_H
#define RMGRP_H

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

namespace CommandRmgrp {

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
        tokens.push_back(item);
    return tokens;
}

inline std::string execute(const std::string& name) {

    if (!currentSession.active)
        return "Error: no hay sesion activa";

    if (currentSession.user != "root")
        return "Error: solo root puede ejecutar rmgrp";

    if (name == "root")
        return "Error: no se puede eliminar el grupo root";

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

    // Reconstruir contenido poniendo id=0 en el grupo
    std::stringstream ss(content);
    std::string line, newContent;
    bool found = false;

    while (std::getline(ss, line)) {
        if (line.empty()) { newContent += "\n"; continue; }

        auto data = split(line, ',');

        if (data.size() >= 3 && data[1] == "G" && data[2] == name && data[0] != "0") {
            newContent += "0,G," + name + "\n";
            found = true;
        } else {
            newContent += line + "\n";
        }
    }

    if (!found) {
        file.close();
        return "Error: el grupo '" + name + "' no existe o ya fue eliminado";
    }

    // Escribir de vuelta (multi-bloque)
    if (!UsersUtils::writeUsersFile(file, sb, partition.start, newContent)) {
        file.close();
        return "Error: no se pudo escribir users.txt";
    }

    file.close();
    return "Grupo eliminado correctamente";
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string token, name;

    while (iss >> token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("-name=") == 0)
            name = token.substr(6);
    }

    if (name.empty())
        return "Error: rmgrp requiere -name";

    return execute(name);
}

} // namespace CommandRmgrp

#endif