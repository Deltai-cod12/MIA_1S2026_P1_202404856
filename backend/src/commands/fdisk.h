#ifndef FDISK_H
#define FDISK_H

/**
 * fdisk.h
 * Parámetros nuevos en para fase 2:
 */

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

inline std::string expandPath(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

inline std::string getParam(const std::string& line, const std::string& key) {
    std::string lline = line, lkey = key;
    std::transform(lline.begin(), lline.end(), lline.begin(), ::tolower);
    std::transform(lkey.begin(),  lkey.end(),  lkey.begin(),  ::tolower);
    size_t pos = lline.find(lkey + "=");
    if (pos == std::string::npos) return "";
    pos += key.size() + 1;
    if (pos >= line.size()) return "";
    if (line[pos] == '"' || line[pos] == '\'') {
        char q = line[pos];
        size_t end = line.find(q, pos + 1);
        if (end == std::string::npos) return line.substr(pos + 1);
        return line.substr(pos + 1, end - pos - 1);
    }
    size_t end = line.find(' ', pos);
    return line.substr(pos, end == std::string::npos ? line.size() - pos : end - pos);
}

// CREAR partición primaria o extendida (P1 sin cambios)
inline std::string createPrimaryOrExtended(
    const std::string& path, int size, char type, char fit, const std::string& name)
{
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";

    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            strncmp(mbr.mbr_partitions[i].part_name, name.c_str(), 16) == 0) {
            file.close();
            return "Error: Ya existe una particion con ese nombre";
        }
    }

    int active = 0; bool hasExtended = false;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            active++;
            if (mbr.mbr_partitions[i].part_type == 'E') hasExtended = true;
        }
    }
    if (active >= 4)             { file.close(); return "Error: Maximo 4 particiones permitidas"; }
    if (type == 'E' && hasExtended) { file.close(); return "Error: Ya existe una particion extendida"; }

    struct Seg { int start, size; };
    std::vector<Seg> used;
    for (int i = 0; i < 4; i++)
        if (mbr.mbr_partitions[i].part_status == '1')
            used.push_back({mbr.mbr_partitions[i].part_start, mbr.mbr_partitions[i].part_s});

    std::sort(used.begin(), used.end(), [](const Seg& a, const Seg& b){ return a.start < b.start; });

    std::vector<Seg> freeSpaces;
    int diskStart = (int)sizeof(MBR), diskEnd = mbr.mbr_tamano;
    if (used.empty()) {
        freeSpaces.push_back({diskStart, diskEnd - diskStart});
    } else {
        if (used[0].start > diskStart) freeSpaces.push_back({diskStart, used[0].start - diskStart});
        for (size_t i = 0; i + 1 < used.size(); i++) {
            int gs = used[i].start + used[i].size, ge = used[i+1].start;
            if (ge > gs) freeSpaces.push_back({gs, ge - gs});
        }
        int le = used.back().start + used.back().size;
        if (diskEnd > le) freeSpaces.push_back({le, diskEnd - le});
    }

    int bestStart = -1;
    if (fit == 'F') {
        for (auto& s : freeSpaces) if (s.size >= size) { bestStart = s.start; break; }
    } else if (fit == 'B') {
        int minW = INT_MAX;
        for (auto& s : freeSpaces) if (s.size >= size && (s.size-size) < minW) { minW = s.size-size; bestStart = s.start; }
    } else {
        int maxS = 0;
        for (auto& s : freeSpaces) if (s.size >= size && s.size > maxS) { maxS = s.size; bestStart = s.start; }
    }
    if (bestStart == -1) { file.close(); return "Error: No hay espacio suficiente"; }

    int slot = -1;
    for (int i = 0; i < 4; i++) if (mbr.mbr_partitions[i].part_status == '0') { slot = i; break; }
    if (slot == -1) { file.close(); return "Error: No hay slot disponible"; }

    mbr.mbr_partitions[slot].part_status = '1';
    mbr.mbr_partitions[slot].part_type   = type;
    mbr.mbr_partitions[slot].part_fit    = fit;
    mbr.mbr_partitions[slot].part_start  = bestStart;
    mbr.mbr_partitions[slot].part_s      = size;
    strncpy(mbr.mbr_partitions[slot].part_name, name.c_str(), 16);

    if (type == 'E') {
        EBR ebr;
        file.seekp(bestStart);
        file.write(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    }

    file.seekp(0);
    file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    return "Particion creada correctamente";
}

// CREAR partición lógica (P1 sin cambios)
inline std::string createLogical(
    const std::string& path, int size, char fit, const std::string& name)
{
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";

    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    int extStart = -1, extSize = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            mbr.mbr_partitions[i].part_type   == 'E') {
            extStart = mbr.mbr_partitions[i].part_start;
            extSize  = mbr.mbr_partitions[i].part_s;
            break;
        }
    }
    if (extStart == -1) { file.close(); return "Error: No existe particion extendida"; }
    int extEnd = extStart + extSize;

    struct Seg { int start, size; };
    std::vector<Seg> usedInExt;
    int prevEBRpos = -1, curEBRpos = extStart;

    while (true) {
        EBR ebr;
        file.seekg(curEBRpos);
        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        if (ebr.part_status == '1' && strncmp(ebr.part_name, name.c_str(), 16) == 0) {
            file.close();
            return "Error: Ya existe una particion logica con ese nombre";
        }
        usedInExt.push_back({curEBRpos, (int)sizeof(EBR)});
        if (ebr.part_status == '1' && ebr.part_size > 0)
            usedInExt.push_back({curEBRpos + (int)sizeof(EBR), ebr.part_size});
        if (ebr.part_next == -1) { prevEBRpos = curEBRpos; break; }
        curEBRpos = ebr.part_next;
    }

    std::sort(usedInExt.begin(), usedInExt.end(),
              [](const Seg& a, const Seg& b){ return a.start < b.start; });
    std::vector<Seg> freeSpaces;
    int cursor = extStart;
    for (auto& u : usedInExt) {
        if (u.start > cursor) freeSpaces.push_back({cursor, u.start - cursor});
        cursor = std::max(cursor, u.start + u.size);
    }
    if (extEnd > cursor) freeSpaces.push_back({cursor, extEnd - cursor});

    int needed = (int)sizeof(EBR) + size;
    int bestStart = -1;
    if (fit == 'F') {
        for (auto& s : freeSpaces) if (s.size >= needed) { bestStart = s.start; break; }
    } else if (fit == 'B') {
        int minW = INT_MAX;
        for (auto& s : freeSpaces) if (s.size >= needed && (s.size-needed) < minW) { minW = s.size-needed; bestStart = s.start; }
    } else {
        int maxS = 0;
        for (auto& s : freeSpaces) if (s.size >= needed && s.size > maxS) { maxS = s.size; bestStart = s.start; }
    }
    if (bestStart == -1) { file.close(); return "Error: No hay espacio suficiente en la particion extendida"; }

    int newEBRpos = bestStart, newDataStart = newEBRpos + (int)sizeof(EBR);
    EBR newEBR;
    newEBR.part_status = '1'; newEBR.part_fit = fit;
    newEBR.part_start = newDataStart; newEBR.part_size = size;
    newEBR.part_next = -1;
    strncpy(newEBR.part_name, name.c_str(), 16);
    file.seekp(newEBRpos);
    file.write(reinterpret_cast<char*>(&newEBR), sizeof(EBR));

    if (prevEBRpos != newEBRpos) {
        EBR prevEBR;
        file.seekg(prevEBRpos);
        file.read(reinterpret_cast<char*>(&prevEBR), sizeof(EBR));
        prevEBR.part_next = newEBRpos;
        file.seekp(prevEBRpos);
        file.write(reinterpret_cast<char*>(&prevEBR), sizeof(EBR));
    }
    file.close();
    return "Particion logica creada correctamente";
}

// P2 — ELIMINAR partición (-delete=fast|full -name= -path=)
inline std::string deletePartition(const std::string& path,
                                    const std::string& name,
                                    const std::string& deleteMode) {
    std::string mode = deleteMode;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    if (mode != "fast" && mode != "full")
        return "Error: -delete acepta 'fast' o 'full'";

    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";

    MBR mbr;
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            strncmp(mbr.mbr_partitions[i].part_name, name.c_str(), 16) == 0) {

            int pStart = mbr.mbr_partitions[i].part_start;
            int pSize  = mbr.mbr_partitions[i].part_s;
            char pType = mbr.mbr_partitions[i].part_type;

            // Si es extendida, también limpiar lógicas
            if (pType == 'E' && mode == "full") {
                // Limpiar toda el área extendida
                file.seekp(pStart);
                for (int b = 0; b < pSize; b++) { char z = '\0'; file.write(&z, 1); }
            }

            // Limpiar el slot en el MBR
            if (mode == "full") {
                file.seekp(pStart);
                for (int b = 0; b < pSize; b++) { char z = '\0'; file.write(&z, 1); }
            }

            // Marcar como inactiva
            mbr.mbr_partitions[i].part_status      = '0';
            mbr.mbr_partitions[i].part_start        = -1;
            mbr.mbr_partitions[i].part_s            = 0;
            mbr.mbr_partitions[i].part_correlative  = -1;
            memset(mbr.mbr_partitions[i].part_name, 0, 16);
            memset(mbr.mbr_partitions[i].part_id,   0,  4);

            file.seekp(0);
            file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            file.close();
            return "Particion '" + name + "' eliminada (" + mode + ")";
        }
    }

    // Buscar en lógicas
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            mbr.mbr_partitions[i].part_type   == 'E') {
            int curPos  = mbr.mbr_partitions[i].part_start;
            int prevPos = -1;
            while (curPos != -1) {
                EBR ebr;
                file.seekg(curPos);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                if (ebr.part_status == '1' &&
                    strncmp(ebr.part_name, name.c_str(), 16) == 0) {
                    // Limpiar datos si full
                    if (mode == "full" && ebr.part_size > 0) {
                        file.seekp(curPos + (int)sizeof(EBR));
                        for (int b = 0; b < ebr.part_size; b++) { char z='\0'; file.write(&z,1); }
                    }
                    // Desenlazar: EBR previo apunta al siguiente de este
                    if (prevPos != -1) {
                        EBR prev;
                        file.seekg(prevPos);
                        file.read(reinterpret_cast<char*>(&prev), sizeof(EBR));
                        prev.part_next = ebr.part_next;
                        file.seekp(prevPos);
                        file.write(reinterpret_cast<char*>(&prev), sizeof(EBR));
                    }
                    // Marcar EBR como libre
                    EBR clean;
                    file.seekp(curPos);
                    file.write(reinterpret_cast<char*>(&clean), sizeof(EBR));
                    file.close();
                    return "Particion logica '" + name + "' eliminada (" + mode + ")";
                }
                prevPos = curPos;
                curPos  = ebr.part_next;
            }
        }
    }
    file.close();
    return "Error: Particion '" + name + "' no encontrada";
}

// P2 — AGREGAR/QUITAR espacio (-add=N -name= -path= [-unit=])
inline std::string addSpace(const std::string& path,
                             const std::string& name,
                             int addBytes) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";

    MBR mbr;
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            strncmp(mbr.mbr_partitions[i].part_name, name.c_str(), 16) == 0) {

            int curSize  = mbr.mbr_partitions[i].part_s;
            int curStart = mbr.mbr_partitions[i].part_start;
            int newSize  = curSize + addBytes;

            if (newSize <= 0) {
                file.close();
                return "Error: no puede quedar espacio negativo en la particion";
            }

            if (addBytes > 0) {
                // Verificar que hay espacio libre después de la partición
                // Buscar si hay otra partición que comience antes de curEnd + addBytes
                int nextStart = mbr.mbr_tamano;

                for (int j = 0; j < 4; j++) {
                    if (j == i) continue;
                    if (mbr.mbr_partitions[j].part_status == '1') {
                        int jStart = mbr.mbr_partitions[j].part_start;

                        if (jStart > curStart && jStart < nextStart) {
                            nextStart = jStart;
                        }
                    }
                }

                int curEnd = curStart + curSize;
                int freeSpace = nextStart - curEnd;

                if (addBytes > freeSpace) {
                    file.close();
                    return "Error: no hay espacio contiguo suficiente para expandir la particion";
                }
                // Verificar que no se sale del disco
                if (curEnd + addBytes > mbr.mbr_tamano) {
                    file.close();
                    return "Error: no hay espacio en el disco para ampliar la particion";
                }
            }

            mbr.mbr_partitions[i].part_s = newSize;
            file.seekp(0);
            file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            file.close();

            std::ostringstream out;
            out << "Particion '" << name << "' ajustada: "
                << curSize << " -> " << newSize << " bytes";
            return out.str();
        }
    }
    file.close();
    return "Error: Particion '" + name + "' no encontrada";
}

// PARSER PRINCIPAL
inline std::string executeFromLine(const std::string& line) {
    std::string path       = getParam(line, "-path");
    std::string name       = getParam(line, "-name");
    std::string deleteMode = getParam(line, "-delete");
    std::string addStr     = getParam(line, "-add");
    std::string sizeStr    = getParam(line, "-size");
    std::string unit       = getParam(line, "-unit");
    std::string type       = getParam(line, "-type");
    std::string fit        = getParam(line, "-fit");

    if (path.empty()) return "Error: fdisk requiere -path";

    std::string expandedPath = expandPath(path);

    // -- P2: DELETE --
    if (!deleteMode.empty()) {
        if (name.empty()) return "Error: -delete requiere -name";
        return deletePartition(expandedPath, name, deleteMode);
    }

    // -- P2: ADD (tiene prioridad sobre -size si ambos están) --
    if (!addStr.empty()) {
        if (name.empty()) return "Error: -add requiere -name";
        int addVal = 0;
        try { addVal = std::stoi(addStr); } catch(...) { return "Error: -add invalido"; }

        // Convertir unidad
        std::string unitLow = unit;
        std::transform(unitLow.begin(), unitLow.end(), unitLow.begin(), ::tolower);
        int addBytes = addVal;
        if      (unitLow == "k") addBytes = addVal * 1024;
        else if (unitLow == "m") addBytes = addVal * 1024 * 1024;
        // 'b' o vacío → bytes

        return addSpace(expandedPath, name, addBytes);
    }

    // -- P1: CREAR --
    if (sizeStr.empty() || name.empty())
        return "Error: Faltan parametros obligatorios (-size, -path, -name)";

    int size = 0;
    try { size = std::stoi(sizeStr); } catch(...) { return "Error: -size invalido"; }
    if (size <= 0) return "Error: -size debe ser positivo";

    std::string unitLow = unit;
    std::transform(unitLow.begin(), unitLow.end(), unitLow.begin(), ::tolower);
    int sizeBytes = size;
    if      (unitLow == "k") sizeBytes = size * 1024;
    else if (unitLow == "m") sizeBytes = size * 1024 * 1024;

    std::string typeLow = type;
    std::transform(typeLow.begin(), typeLow.end(), typeLow.begin(), ::tolower);

    char partFit = 'F';
    std::string fitLow = fit;
    std::transform(fitLow.begin(), fitLow.end(), fitLow.begin(), ::tolower);
    if      (fitLow == "bf") partFit = 'B';
    else if (fitLow == "wf") partFit = 'W';

    if (typeLow == "l") return createLogical(expandedPath, sizeBytes, partFit, name);

    char partType = 'P';
    if (typeLow == "e") partType = 'E';
    return createPrimaryOrExtended(expandedPath, sizeBytes, partType, partFit, name);
}

} // namespace CommandFdisk

#endif // FDISK_H