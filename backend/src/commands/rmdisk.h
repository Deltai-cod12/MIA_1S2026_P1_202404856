#ifndef RMDISK_H
#define RMDISK_H

#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../models/structs.h"

namespace CommandRmdisk {

    /*
        Expandir rutas que contienen ~
    */
    inline std::string expandPath(const std::string& path) {

        if (path.empty() || path[0] != '~')
            return path;

        const char* home = std::getenv("HOME");

        if (!home)
            return path;

        return std::string(home) + path.substr(1);

    }

    /*
        Comando rmdisk
        Elimina un disco virtual
    */
    inline std::string execute(const std::string& path) {

        if (path.empty())
            return "Error: falta parametro -path";

        std::string expandedPath = expandPath(path);

        /*
            Verificar que el disco exista
        */
        std::ifstream check(expandedPath);

        if (!check.good())
            return "Error: el disco no existe";

        check.close();

        /*
            Leer MBR antes de eliminar
        */
        std::ifstream file(expandedPath, std::ios::binary);

        if (!file.is_open())
            return "Error: no se pudo abrir el disco";

        MBR mbr;

        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        file.close();

        /*
            Formatear fecha
        */
        char fecha[100];

        strftime(
            fecha,
            sizeof(fecha),
            "%d/%m/%Y %H:%M",
            localtime(&mbr.mbr_fecha_creacion)
        );

        /*
            Eliminar archivo
        */
        if (remove(expandedPath.c_str()) != 0)
            return "Error: no se pudo eliminar el disco";

        std::ostringstream result;

        result << "\n=== RMDISK ===\n";
        result << "Disco eliminado correctamente\n";
        result << "  Ruta: " << expandedPath << "\n";
        result << "  Tamaño: " << mbr.mbr_tamano << " bytes\n";
        result << "  Fecha creación: " << fecha << "\n";
        result << "  Firma: " << mbr.mbr_dsk_signature;

        return result.str();

    }

}

#endif