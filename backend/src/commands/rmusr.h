#ifndef RMUSR_H
#define RMUSR_H

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

namespace CommandRmusr {

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
        tokens.push_back(item);
    return tokens;
}

inline std::string execute(const std::string& user) {

    if (!currentSession.active)
        return "Error: no hay sesion activa";

    if (currentSession.user != "root")
        return "Error: solo root puede ejecutar rmusr";

    if (user == "root")
        return "Error: no se puede eliminar el usuario root";

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

    // Reconstruir contenido poniendo id=0 en el usuario
    std::stringstream ss(content);
    std::string line, newContent;
    bool found = false;

    while (std::getline(ss, line)) {
        if (line.empty()) { newContent += "\n"; continue; }

        auto data = split(line, ',');

        // formato: id,U,grupo,usuario,pass
        if (data.size() == 5 && data[1] == "U" && data[3] == user && data[0] != "0") {
            newContent += "0,U," + data[2] + "," + data[3] + "," + data[4] + "\n";
            found = true;
        } else {
            newContent += line + "\n";
        }
    }

    if (!found) {
        file.close();
        return "Error: el usuario '" + user + "' no existe o ya fue eliminado";
    }

    // Escribir de vuelta (multi-bloque)
    if (!UsersUtils::writeUsersFile(file, sb, partition.start, newContent)) {
        file.close();
        return "Error: no se pudo escribir users.txt";
    }

    file.close();
    return "Usuario eliminado correctamente";
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string token, user;

    while (iss >> token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("-user=") == 0)
            user = token.substr(6);
    }

    if (user.empty())
        return "Error: rmusr requiere -user";

    return execute(user);
}

} // namespace CommandRmusr

#endif