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
#include <algorithm>
#include "../models/structs.h"

namespace CommandMkdisk {

inline std::string expandPath(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

inline bool createDirectories(const std::string& path) {
    try {
        std::filesystem::path filePath(path);
        std::filesystem::path parentPath = filePath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath))
            std::filesystem::create_directories(parentPath);
        return true;
    } catch (...) {
        return false;
    }
}

inline std::string createDisk(int size, const std::string& unit,
                               const std::string& path, char fit) {
    if (size <= 0)
        return "Error: El tamaño debe ser mayor a 0";

    std::string unitLow = unit;
    std::transform(unitLow.begin(), unitLow.end(), unitLow.begin(), ::tolower);

    int sizeBytes = size;
    if      (unitLow == "k") sizeBytes = size * 1024;
    else if (unitLow == "m") sizeBytes = size * 1024 * 1024;
    else if (unitLow == "b" || unitLow.empty()) sizeBytes = size;
    else return "Error: Unidad invalida (use B, K o M)";

    std::string realPath = expandPath(path);

    if (realPath.length() < 4 ||
        realPath.substr(realPath.length() - 4) != ".mia")
        return "Error: El disco debe tener extension .mia";

    if (!createDirectories(realPath))
        return "Error: No se pudieron crear las carpetas del disco";

    std::ifstream check(realPath);
    if (check.good()) { check.close(); return "Error: El disco ya existe"; }
    check.close();

    std::ofstream file(realPath, std::ios::binary);
    if (!file.is_open()) return "Error: No se pudo crear el disco";

    file.seekp(sizeBytes - 1);
    char zero = '\0';
    file.write(&zero, 1);

    MBR mbr;
    mbr.mbr_tamano         = sizeBytes;
    mbr.mbr_fecha_creacion = time(nullptr);
    mbr.mbr_dsk_signature  = rand();
    mbr.dsk_fit            = fit;

    file.seekp(0);
    file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();

    char buffer[64];
    std::tm* ti = std::localtime(&mbr.mbr_fecha_creacion);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ti);

    return "Disco creado exitosamente\n"
           "Ruta: "   + realPath + "\n"
           "Tamaño: " + std::to_string(sizeBytes) + " bytes\n"
           "Fecha: "  + std::string(buffer) + "\n"
           "Firma: "  + std::to_string(mbr.mbr_dsk_signature);
}

// ─── Parser con validación de parámetros desconocidos ────────────────────────

inline std::string execute(const std::string& commandLine) {

    std::istringstream iss(commandLine);
    std::string token;

    std::string sizeStr, path, unit, fitStr;
    bool hasUnknown = false;
    std::string unknownParam;

    iss >> token;  // saltar "mkdisk"

    while (iss >> token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if      (lower.find("-size=") == 0) sizeStr = token.substr(6);
        else if (lower.find("-path=") == 0) path    = token.substr(6);
        else if (lower.find("-unit=") == 0) unit    = token.substr(6);
        else if (lower.find("-fit=")  == 0) fitStr  = token.substr(5);
        else {
            // Cualquier token que empiece con '-' y no sea conocido → error
            if (!token.empty() && token[0] == '-') {
                hasUnknown   = true;
                unknownParam = token;
            }
        }
    }

    if (hasUnknown)
        return "Error: Parametro desconocido '" + unknownParam +
               "'. Parametros validos: -size, -path, -unit, -fit";

    if (sizeStr.empty() || path.empty())
        return "Error: Faltan parametros obligatorios (-size y -path son requeridos)";

    int size = 0;
    try { size = std::stoi(sizeStr); }
    catch (...) { return "Error: -size debe ser un numero entero"; }

    if (unit.empty()) unit = "M";  // default: megabytes

    char fit = 'F';
    std::string fitLow = fitStr;
    std::transform(fitLow.begin(), fitLow.end(), fitLow.begin(), ::tolower);
    if      (fitLow == "bf") fit = 'B';
    else if (fitLow == "wf") fit = 'W';

    return createDisk(size, unit, path, fit);
}

}  // namespace CommandMkdisk

#endif