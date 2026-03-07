#ifndef FDISK_H  
#define FDISK_H   

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <climits>
#include "../models/structs.h"

namespace CommandFdisk {

    // Expandir ~
    inline std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') return path;
        const char* home = std::getenv("HOME");
        if (!home) return path;
        return std::string(home) + path.substr(1);
    }

    // ==============================
    // CREAR PRIMARIA O EXTENDIDA
    // ==============================
    inline std::string createPrimaryOrExtended(
        const std::string& path,
        int size,
        char type,
        char fit,
        const std::string& name)
    {
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open())
            return "Error: No se pudo abrir el disco";

        MBR mbr;
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        // Validar nombre único
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' &&
                strcmp(mbr.mbr_partitions[i].part_name, name.c_str()) == 0)
            {
                file.close();
                return "Error: Ya existe una partición con ese nombre";
            }
        }

        int active = 0;
        bool hasExtended = false;

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                active++;
                if (mbr.mbr_partitions[i].part_type == 'E')
                    hasExtended = true;
            }
        }

        if (active >= 4) {
            file.close();
            return "Error: Máximo 4 particiones permitidas";
        }

        if (type == 'E' && hasExtended) {
            file.close();
            return "Error: Ya existe una partición extendida";
        }

        // Calcular espacios libres reales

        struct Used {
            int start;
            int size;
        };

        std::vector<Used> used;

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                used.push_back({
                    mbr.mbr_partitions[i].part_start,
                    mbr.mbr_partitions[i].part_s
                });
            }
        }

        std::sort(used.begin(), used.end(),
            [](const Used& a, const Used& b) {
                return a.start < b.start;
            });

        struct Free {
            int start;
            int size;
        };

        std::vector<Free> freeSpaces;

        int diskStart = sizeof(MBR);
        int diskEnd = mbr.mbr_tamano;

        if (used.empty()) {
            freeSpaces.push_back({diskStart, diskEnd - diskStart});
        } else {

            if (used[0].start > diskStart) {
                freeSpaces.push_back({
                    diskStart,
                    used[0].start - diskStart
                });
            }

            for (size_t i = 0; i < used.size() - 1; i++) {
                int gapStart = used[i].start + used[i].size;
                int gapEnd = used[i+1].start;

                if (gapEnd > gapStart) {
                    freeSpaces.push_back({
                        gapStart,
                        gapEnd - gapStart
                    });
                }
            }

            int lastEnd = used.back().start + used.back().size;
            if (diskEnd > lastEnd) {
                freeSpaces.push_back({
                    lastEnd,
                    diskEnd - lastEnd
                });
            }
        }

        int bestStart = -1;

        if (fit == 'F') {
            for (auto& s : freeSpaces)
                if (s.size >= size) { bestStart = s.start; break; }
        }
        else if (fit == 'B') {
            int minWaste = INT_MAX;
            for (auto& s : freeSpaces)
                if (s.size >= size) {
                    int waste = s.size - size;
                    if (waste < minWaste) {
                        minWaste = waste;
                        bestStart = s.start;
                    }
                }
        }
        else {
            int maxSpace = 0;
            for (auto& s : freeSpaces)
                if (s.size >= size && s.size > maxSpace) {
                    maxSpace = s.size;
                    bestStart = s.start;
                }
        }

        if (bestStart == -1) {
            file.close();
            return "Error: No hay espacio suficiente";
        }

        int slot = -1;
        for (int i = 0; i < 4; i++)
            if (mbr.mbr_partitions[i].part_status == '0') {
                slot = i;
                break;
            }

        if (slot == -1) {
            file.close();
            return "Error: No hay slot disponible";
        }

        mbr.mbr_partitions[slot].part_status = '1';
        mbr.mbr_partitions[slot].part_type = type;
        mbr.mbr_partitions[slot].part_fit = fit;
        mbr.mbr_partitions[slot].part_start = bestStart;
        mbr.mbr_partitions[slot].part_s = size;
        strncpy(mbr.mbr_partitions[slot].part_name, name.c_str(), 16);

        if (type == 'E') {
            EBR ebr;
            file.seekp(bestStart);
            file.write(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        }

        file.seekp(0);
        file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        file.close();

        return "Partición creada correctamente";
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

    inline std::string executeFromLine(const std::string& line) {

        std::string sizeStr = getParam(line, "-size");
        std::string path = getParam(line, "-path");
        std::string name = getParam(line, "-name");
        std::string unit = getParam(line, "-unit");
        std::string type = getParam(line, "-type");
        std::string fit = getParam(line, "-fit");

        if (sizeStr.empty() || path.empty() || name.empty())
            return "Error: Faltan parámetros obligatorios";

        int size = std::stoi(sizeStr);

        int sizeBytes = size;
        if (unit == "k" || unit == "K")
            sizeBytes *= 1024;
        else if (unit == "m" || unit == "M")
            sizeBytes *= 1024 * 1024;

        char partType = 'P';
        if (type == "e" || type == "E") partType = 'E';

        char partFit = 'F';
        if (fit == "bf" || fit == "BF") partFit = 'B';
        if (fit == "wf" || fit == "WF") partFit = 'W';

        return createPrimaryOrExtended(
            expandPath(path),
            sizeBytes,
            partType,
            partFit,
            name
        );
    }

}

#endif