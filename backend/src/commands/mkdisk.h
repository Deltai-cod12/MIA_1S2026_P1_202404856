#ifndef MKDISK_H
#define MKDISK_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <ctime>
#include "../models/structs.h"

namespace CommandMkdisk {

    // Expandir rutas con ~
    inline std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') return path;
        const char* home = std::getenv("HOME");
        if (!home) return path;
        return std::string(home) + path.substr(1);
    }

    // Crear directorios padre si no existen
    inline bool createDirectories(const std::string& path) {
        try {
            std::filesystem::path filePath(path);
            std::filesystem::path parentPath = filePath.parent_path();

            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                std::filesystem::create_directories(parentPath);
            }

            return true;
        }
        catch (...) {
            return false;
        }
    }

    // ==============================
    // CREAR DISCO
    // ==============================
    inline std::string createDisk(int size, const std::string& unit, const std::string& path) {

        if (size <= 0)
            return "Error: El tamaño debe ser mayor a 0";

        int sizeBytes = size;

        if (unit == "k" || unit == "K")
            sizeBytes *= 1024;
        else if (unit == "m" || unit == "M")
            sizeBytes *= 1024 * 1024;
        else
            return "Error: Unidad inválida (use k o m)";

        std::string realPath = expandPath(path);

        if (!createDirectories(realPath))
            return "Error: No se pudieron crear carpetas";

        std::ifstream check(realPath);
        if (check.good()) {
            check.close();
            return "Error: El disco ya existe";
        }
        check.close();

        std::ofstream file(realPath, std::ios::binary);
        if (!file.is_open())
            return "Error: No se pudo crear el disco";

        // Reservar tamaño del disco
        file.seekp(sizeBytes - 1);
        file.write("", 1);

        // Crear MBR usando tu struct
        MBR mbr;
        mbr.mbr_tamano = sizeBytes;
        mbr.mbr_fecha_creacion = time(nullptr);
        mbr.mbr_dsk_signature = rand();
        mbr.dsk_fit = 'F';

        // Las particiones ya vienen inicializadas por el constructor

        file.seekp(0);
        file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        file.close();

        char buffer[64];
        std::tm* timeinfo = std::localtime(&mbr.mbr_fecha_creacion);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

        return "Disco creado exitosamente\n"
               "Ruta: " + realPath + "\n" +
               "Tamaño: " + std::to_string(sizeBytes) + " bytes\n" +
               "Fecha: " + std::string(buffer) + "\n" +
               "Firma: " + std::to_string(mbr.mbr_dsk_signature);
    }

    // ==============================
    // PARSEAR LINEA COMPLETA
    // ==============================
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

    inline std::string execute(const std::string& line) {

        std::string sizeStr = getParam(line, "-size");
        std::string path = getParam(line, "-path");
        std::string unit = getParam(line, "-unit");

        if (sizeStr.empty() || path.empty())
            return "Error: Faltan parámetros obligatorios";

        int size = std::stoi(sizeStr);

        if (unit.empty())
            unit = "m";

        return createDisk(size, unit, path);
    }

}

#endif