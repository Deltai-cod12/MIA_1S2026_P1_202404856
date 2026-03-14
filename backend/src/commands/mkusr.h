#ifndef MKUSR_H
#define MKUSR_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

#include "../session/session.h"
#include "../commands/mount.h"
#include "../models/structs.h"
#include "../commands/users_utils.h"

namespace CommandMkusr {

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
                           const std::string& grp) {

    if (!currentSession.active)
        return "Error: no hay sesion activa";

    if (currentSession.user != "root")
        return "Error: solo root puede ejecutar mkusr";

    if (user.empty() || user.length() > 10)
        return "Error: user invalido (max 10 caracteres)";
    if (pass.empty() || pass.length() > 10)
        return "Error: pass invalido (max 10 caracteres)";
    if (grp.empty() || grp.length() > 10)
        return "Error: grp invalido (max 10 caracteres)";

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

    // Parsear líneas
    std::stringstream ss(content);
    std::string line;
    bool groupExists = false;
    int lastId = 0;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        auto data = split(line, ',');
        if (data.size() < 3) continue;

        int id = 0;
        try { id = std::stoi(data[0]); } catch (...) { continue; }

        // Verificar que el grupo destino existe y no fue eliminado
        if (data[1] == "G" && data[2] == grp && id != 0)
            groupExists = true;

        // Verificar que el usuario no exista ya (activo)
        if (data[1] == "U" && data.size() >= 5 && data[3] == user && id != 0) {
            file.close();
            return "Error: el usuario '" + user + "' ya existe";
        }

        // Rastrear el último ID de usuario para generar el siguiente
        if (data[1] == "U" && id > lastId)
            lastId = id;
    }

    if (!groupExists) {
        file.close();
        return "Error: el grupo '" + grp + "' no existe";
    }

    // Agregar nueva línea
    int newId = lastId + 1;
    content += std::to_string(newId) + ",U," + grp + "," + user + "," + pass + "\n";

    // Escribir de vuelta (multi-bloque)
    if (!UsersUtils::writeUsersFile(file, sb, partition.start, content)) {
        file.close();
        return "Error: no hay espacio en disco para agregar el usuario";
    }

    file.close();
    return "Usuario creado correctamente";
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string token;
    std::string user, pass, grp;

    while (iss >> token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if      (lower.find("-user=") == 0) user = token.substr(6);
        else if (lower.find("-pass=") == 0) pass = token.substr(6);
        else if (lower.find("-grp=")  == 0) grp  = token.substr(5);
    }

    if (user.empty() || pass.empty() || grp.empty())
        return "Error: mkusr requiere -user -pass -grp";

    return execute(user, pass, grp);
}

} // namespace CommandMkusr

#endif