#ifndef REP_H
#define REP_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <cstring>
#include <iomanip>
#include <sys/stat.h>
#include <cstdlib>

#include "../models/structs.h"
#include "mount.h"

namespace CommandRep {

    inline std::string toLowerCase(const std::string& str) {
        std::string r = str;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        return r;
    }

    /*
        Crear directorios recursivamente
    */
    inline void createDirectories(const std::string& path) {

        std::string cmd = "mkdir -p \"" + path + "\"";
        system(cmd.c_str());

    }

    /*
        Obtener carpeta padre
    */
    inline std::string getParentPath(const std::string& path) {

        size_t pos = path.find_last_of('/');

        if (pos == std::string::npos)
            return ".";

        return path.substr(0, pos);

    }

    /*
        Obtener extensión
    */
    inline std::string getExtension(const std::string& path) {

        size_t pos = path.find_last_of('.');

        if (pos == std::string::npos)
            return "png";

        return path.substr(pos + 1);

    }

    /*
        Limpiar nombre char[16]
    */
    inline std::string cleanName(char name[16]) {

        std::string s(name);

        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());

        return s;

    }

    /*
        REPORTE MBR
    */
    inline std::string reportMBR(const std::string& outputPath, const std::string& diskPath) {

        std::ifstream file(diskPath, std::ios::binary);

        if (!file.is_open())
            return "Error: no se pudo abrir el disco";

        MBR mbr;

        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        std::ostringstream dot;

        dot << "digraph G{\n";
        dot << "node[shape=plaintext]\n";
        dot << "tabla[label=<\n";
        dot << "<table border='1' cellborder='1'>\n";

        dot << "<tr><td colspan='2'><b>MBR</b></td></tr>\n";

        dot << "<tr><td>mbr_tamano</td><td>" << mbr.mbr_tamano << "</td></tr>\n";

        char fecha[100];
        strftime(fecha, sizeof(fecha), "%d/%m/%Y %H:%M", localtime(&mbr.mbr_fecha_creacion));

        dot << "<tr><td>mbr_fecha_creacion</td><td>" << fecha << "</td></tr>\n";

        dot << "<tr><td>mbr_disk_signature</td><td>" << mbr.mbr_dsk_signature << "</td></tr>\n";
        dot << "<tr><td>dsk_fit</td><td>" << mbr.dsk_fit << "</td></tr>\n";

        for (int i = 0; i < 4; i++) {

            Partition part = mbr.mbr_partitions[i];

            if (part.part_status != '1')
                continue;

            dot << "<tr><td colspan='2'><b>PARTICION</b></td></tr>\n";

            dot << "<tr><td>status</td><td>" << part.part_status << "</td></tr>\n";
            dot << "<tr><td>type</td><td>" << part.part_type << "</td></tr>\n";
            dot << "<tr><td>fit</td><td>" << part.part_fit << "</td></tr>\n";
            dot << "<tr><td>start</td><td>" << part.part_start << "</td></tr>\n";
            dot << "<tr><td>size</td><td>" << part.part_s << "</td></tr>\n";
            dot << "<tr><td>name</td><td>" << cleanName(part.part_name) << "</td></tr>\n";

        }

        dot << "</table>>];\n";
        dot << "}";

        file.close();

        std::string parent = getParentPath(outputPath);
        createDirectories(parent);

        std::string dotPath = outputPath + ".dot";

        std::ofstream dotFile(dotPath);

        dotFile << dot.str();
        dotFile.close();

        std::string ext = getExtension(outputPath);

        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + outputPath + "\"";

        system(cmd.c_str());

        remove(dotPath.c_str());

        return "Reporte MBR generado: " + outputPath;

    }

    /*
        REPORTE DISK
    */
    inline std::string reportDISK(const std::string& outputPath, const std::string& diskPath) {

        std::ifstream file(diskPath, std::ios::binary);

        if (!file.is_open())
            return "Error: no se pudo abrir el disco";

        MBR mbr;

        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        int diskSize = mbr.mbr_tamano;

        std::ostringstream dot;

        dot << "digraph G{\n";
        dot << "node[shape=plaintext]\n";

        dot << "tabla[label=<\n";
        dot << "<table border='1' cellborder='1'>\n<tr>";

        dot << "<td>MBR</td>";

        for (int i = 0; i < 4; i++) {

            Partition part = mbr.mbr_partitions[i];

            if (part.part_status != '1')
                continue;

            double percent = (double)part.part_s * 100 / diskSize;

            if (part.part_type == 'E') {

                dot << "<td>Extendida<br/>"
                    << std::fixed << std::setprecision(2)
                    << percent << "%</td>";

            } else {

                dot << "<td>Primaria<br/>"
                    << std::fixed << std::setprecision(2)
                    << percent << "%</td>";

            }

        }

        dot << "</tr></table>>];\n";
        dot << "}";

        file.close();

        std::string parent = getParentPath(outputPath);
        createDirectories(parent);

        std::string dotPath = outputPath + ".dot";

        std::ofstream dotFile(dotPath);

        dotFile << dot.str();
        dotFile.close();

        std::string ext = getExtension(outputPath);

        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + outputPath + "\"";

        system(cmd.c_str());

        remove(dotPath.c_str());

        return "Reporte DISK generado: " + outputPath;

    }

    /*
        COMANDO REP
    */
    inline std::string execute(
        const std::string& name,
        const std::string& path,
        const std::string& id
    ) {

        if (name.empty())
            return "Error: falta -name";

        if (path.empty())
            return "Error: falta -path";

        if (id.empty())
            return "Error: falta -id";

        CommandMount::MountedPartition partition;

        if (!CommandMount::getMountedPartition(id, partition))
            return "Error: la partición no está montada";

        std::string type = toLowerCase(name);

        if (type == "mbr")
            return reportMBR(path, partition.path);

        if (type == "disk")
            return reportDISK(path, partition.path);

        return "Reporte no implementado";

    }

}

#endif