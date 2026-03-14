#ifndef RMDISK_H
#define RMDISK_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <ctime>

#include "../models/structs.h"

namespace CommandRmdisk {

    // Expandir rutas con ~
    inline std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') return path;
        const char* home = std::getenv("HOME");
        if (!home) return path;
        return std::string(home) + path.substr(1);
    }

    // Obtener parametro (igual que mkdisk)
    inline std::string getParam(const std::string& line, const std::string& key) {

        size_t pos = line.find(key + "=");
        if (pos == std::string::npos) return "";

        pos += key.length() + 1;

        if (line[pos] == '"') {
            size_t end = line.find('"', pos + 1);
            return line.substr(pos + 1, end - pos - 1);
        }

        size_t end = line.find(" ", pos);
        return line.substr(pos, end - pos);
    }

    // Ejecutar rmdisk
    inline std::string removeDisk(const std::string& path) {

        std::string realPath = expandPath(path);

        // Validar extension
        if (realPath.length() < 4 || realPath.substr(realPath.length() - 4) != ".mia") {
            return "Error: El disco debe tener extensión .mia";
        }

        // Verificar existencia
        if (!std::filesystem::exists(realPath)) {
            return "Error: el disco no existe";
        }

        // Leer MBR antes de eliminar
        std::ifstream file(realPath, std::ios::binary);

        if (!file.is_open())
            return "Error: no se pudo abrir el disco";

        MBR mbr;
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        file.close();

        // Eliminar archivo
        if (!std::filesystem::remove(realPath))
            return "Error: no se pudo eliminar el disco";

        return "Disco eliminado correctamente\nRuta: " + realPath;
    }

    // Ejecutar comando completo
    
    inline std::string execute(const std::string& line) {

        std::string path = getParam(line, "-path");

        if (path.empty())
            return "Error: falta parametro -path";

        return removeDisk(path);
    }

}

#endif