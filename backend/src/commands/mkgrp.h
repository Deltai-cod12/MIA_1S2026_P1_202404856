#ifndef MKGRP_H
#define MKGRP_H

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
#include "../commands/users_utils.h"   //  helper multi-bloque

namespace CommandMkgrp {

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
        return "Error: solo root puede ejecutar mkgrp";

    if (name.empty() || name.length() > 10)
        return "Error: nombre de grupo invalido (max 10 caracteres)";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(currentSession.id, partition))
        return "Error: particion no montada";

    std::fstream file(partition.path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco";

    // Leer superbloque
    Superblock sb;
    file.seekg(partition.start);
    file.read((char*)&sb, sizeof(Superblock));

    // Leer users.txt completo (soporta múltiples bloques)
    std::string content = UsersUtils::readUsersFile(file, sb);

    // Parsear líneas
    int lastId = 0;
    std::stringstream ss(content);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        auto data = split(line, ',');
        if (data.size() < 3) continue;

        int id = 0;
        try { id = std::stoi(data[0]); } catch (...) { continue; }

        if (id > lastId) lastId = id;

        // Verificar que el grupo no exista (y no esté eliminado, id != 0)
        if (data[1] == "G" && data[2] == name && id != 0) {
            file.close();
            return "Error: el grupo '" + name + "' ya existe";
        }
    }

    // Agregar nueva línea al contenido
    int newId = lastId + 1;
    content += std::to_string(newId) + ",G," + name + "\n";

    // Escribir de vuelta (con soporte multi-bloque)
    if (!UsersUtils::writeUsersFile(file, sb, partition.start, content)) {
        file.close();
        return "Error: no hay espacio en disco para agregar el grupo";
    }

    file.close();
    return "Grupo creado correctamente";
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
        return "Error: mkgrp requiere -name";

    return execute(name);
}

} // namespace CommandMkgrp

#endif