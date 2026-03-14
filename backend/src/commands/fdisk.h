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

inline std::string expandPath(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

// ─── PRIMARIA O EXTENDIDA ────────────────────────────────────────────────────

inline std::string createPrimaryOrExtended(
    const std::string& path, int size, char type, char fit, const std::string& name)
{
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";

    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // Nombre único entre primarias/extendidas
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            strncmp(mbr.mbr_partitions[i].part_name, name.c_str(), 16) == 0) {
            file.close();
            return "Error: Ya existe una particion con ese nombre";
        }
    }

    int active = 0;
    bool hasExtended = false;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            active++;
            if (mbr.mbr_partitions[i].part_type == 'E') hasExtended = true;
        }
    }

    if (active >= 4)            { file.close(); return "Error: Maximo 4 particiones permitidas"; }
    if (type == 'E' && hasExtended) { file.close(); return "Error: Ya existe una particion extendida"; }

    // Calcular espacios libres
    struct Seg { int start, size; };
    std::vector<Seg> used;
    for (int i = 0; i < 4; i++)
        if (mbr.mbr_partitions[i].part_status == '1')
            used.push_back({mbr.mbr_partitions[i].part_start, mbr.mbr_partitions[i].part_s});

    std::sort(used.begin(), used.end(), [](const Seg& a, const Seg& b){ return a.start < b.start; });

    std::vector<Seg> freeSpaces;
    int diskStart = (int)sizeof(MBR);
    int diskEnd   = mbr.mbr_tamano;

    if (used.empty()) {
        freeSpaces.push_back({diskStart, diskEnd - diskStart});
    } else {
        if (used[0].start > diskStart)
            freeSpaces.push_back({diskStart, used[0].start - diskStart});
        for (size_t i = 0; i + 1 < used.size(); i++) {
            int gapStart = used[i].start + used[i].size;
            int gapEnd   = used[i+1].start;
            if (gapEnd > gapStart) freeSpaces.push_back({gapStart, gapEnd - gapStart});
        }
        int lastEnd = used.back().start + used.back().size;
        if (diskEnd > lastEnd) freeSpaces.push_back({lastEnd, diskEnd - lastEnd});
    }

    int bestStart = -1;
    if (fit == 'F') {
        for (auto& s : freeSpaces)
            if (s.size >= size) { bestStart = s.start; break; }
    } else if (fit == 'B') {
        int minW = INT_MAX;
        for (auto& s : freeSpaces)
            if (s.size >= size && (s.size - size) < minW) { minW = s.size - size; bestStart = s.start; }
    } else {
        int maxS = 0;
        for (auto& s : freeSpaces)
            if (s.size >= size && s.size > maxS) { maxS = s.size; bestStart = s.start; }
    }

    if (bestStart == -1) { file.close(); return "Error: No hay espacio suficiente"; }

    int slot = -1;
    for (int i = 0; i < 4; i++)
        if (mbr.mbr_partitions[i].part_status == '0') { slot = i; break; }
    if (slot == -1) { file.close(); return "Error: No hay slot disponible"; }

    mbr.mbr_partitions[slot].part_status = '1';
    mbr.mbr_partitions[slot].part_type   = type;
    mbr.mbr_partitions[slot].part_fit    = fit;
    mbr.mbr_partitions[slot].part_start  = bestStart;
    mbr.mbr_partitions[slot].part_s      = size;
    strncpy(mbr.mbr_partitions[slot].part_name, name.c_str(), 16);

    // Si es extendida, escribir el primer EBR vacío al inicio
    if (type == 'E') {
        EBR ebr;  // constructor inicializa: status='0', start=-1, size=0, next=-1
        file.seekp(bestStart);
        file.write(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    }

    file.seekp(0);
    file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    return "Particion creada correctamente";
}

// ─── LÓGICA ──────────────────────────────────────────────────────────────────

inline std::string createLogical(
    const std::string& path, int size, char fit, const std::string& name)
{
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";

    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // Encontrar la partición extendida
    int extStart = -1, extSize = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' &&
            mbr.mbr_partitions[i].part_type   == 'E') {
            extStart = mbr.mbr_partitions[i].part_start;
            extSize  = mbr.mbr_partitions[i].part_s;
            break;
        }
    }

    if (extStart == -1) {
        file.close();
        return "Error: No existe particion extendida";
    }

    int extEnd = extStart + extSize;

    // ── Recorrer cadena de EBRs ──────────────────────────────────────────────
    int prevEBRpos = -1;   // posición del EBR anterior (para actualizar part_next)
    int curEBRpos  = extStart;

    // Mapa de bloques ocupados dentro de la extendida: {start, size}
    struct Seg { int start, size; };
    std::vector<Seg> usedInExt;

    while (true) {
        EBR ebr;
        file.seekg(curEBRpos);
        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

        // Verificar nombre único
        if (ebr.part_status == '1' &&
            strncmp(ebr.part_name, name.c_str(), 16) == 0) {
            file.close();
            return "Error: Ya existe una particion logica con ese nombre";
        }

        // Registrar espacio ocupado: el EBR mismo + sus datos
        usedInExt.push_back({curEBRpos, (int)sizeof(EBR)});
        if (ebr.part_status == '1' && ebr.part_size > 0)
            usedInExt.push_back({curEBRpos + (int)sizeof(EBR), ebr.part_size});

        if (ebr.part_next == -1) {
            // Llegamos al último EBR
            prevEBRpos = curEBRpos;
            break;
        }
        curEBRpos = ebr.part_next;
    }

    // ── Calcular espacios libres dentro de la extendida ──────────────────────
    std::sort(usedInExt.begin(), usedInExt.end(),
              [](const Seg& a, const Seg& b){ return a.start < b.start; });

    std::vector<Seg> freeSpaces;

    // Primer espacio libre: justo después de todos los EBRs/datos ya usados
    int cursor = extStart;
    for (auto& u : usedInExt) {
        if (u.start > cursor)
            freeSpaces.push_back({cursor, u.start - cursor});
        cursor = std::max(cursor, u.start + u.size);
    }
    if (extEnd > cursor)
        freeSpaces.push_back({cursor, extEnd - cursor});

    // Necesitamos espacio para EBR + datos
    int needed = (int)sizeof(EBR) + size;

    int bestStart = -1;
    if (fit == 'F') {
        for (auto& s : freeSpaces)
            if (s.size >= needed) { bestStart = s.start; break; }
    } else if (fit == 'B') {
        int minW = INT_MAX;
        for (auto& s : freeSpaces)
            if (s.size >= needed && (s.size - needed) < minW) {
                minW = s.size - needed; bestStart = s.start;
            }
    } else {
        int maxS = 0;
        for (auto& s : freeSpaces)
            if (s.size >= needed && s.size > maxS) { maxS = s.size; bestStart = s.start; }
    }

    if (bestStart == -1) {
        file.close();
        return "Error: No hay espacio suficiente en la particion extendida";
    }

    // ── Crear el nuevo EBR ───────────────────────────────────────────────────
    int newEBRpos   = bestStart;
    int newDataStart = newEBRpos + (int)sizeof(EBR);

    EBR newEBR;
    newEBR.part_status = '1';
    newEBR.part_fit    = fit;
    newEBR.part_start  = newDataStart;
    newEBR.part_size   = size;
    newEBR.part_next   = -1;
    strncpy(newEBR.part_name, name.c_str(), 16);

    file.seekp(newEBRpos);
    file.write(reinterpret_cast<char*>(&newEBR), sizeof(EBR));

    // ── Enlazar: el EBR anterior apunta al nuevo ─────────────────────────────
    if (prevEBRpos != newEBRpos) {
        // Leer EBR anterior, actualizar part_next y reescribir
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

// ─── PARSER ──────────────────────────────────────────────────────────────────

inline std::string getParam(const std::string& line, const std::string& key) {
    // Búsqueda case-insensitive
    std::string lline = line;
    std::transform(lline.begin(), lline.end(), lline.begin(), ::tolower);
    std::string lkey = key;
    std::transform(lkey.begin(), lkey.end(), lkey.begin(), ::tolower);

    size_t pos = lline.find(lkey + "=");
    if (pos == std::string::npos) return "";
    pos += key.length() + 1;

    if (pos >= line.size()) return "";

    if (line[pos] == '"') {
        size_t end = line.find('"', pos + 1);
        if (end == std::string::npos) return line.substr(pos + 1);
        return line.substr(pos + 1, end - pos - 1);
    }

    size_t end = line.find(' ', pos);
    return line.substr(pos, end - pos);
}

inline std::string executeFromLine(const std::string& line) {

    std::string sizeStr = getParam(line, "-size");
    std::string path    = getParam(line, "-path");
    std::string name    = getParam(line, "-name");
    std::string unit    = getParam(line, "-unit");
    std::string type    = getParam(line, "-type");
    std::string fit     = getParam(line, "-fit");

    if (sizeStr.empty() || path.empty() || name.empty())
        return "Error: Faltan parametros obligatorios (-size, -path, -name)";

    int size = 0;
    try { size = std::stoi(sizeStr); } catch (...) { return "Error: -size invalido"; }
    if (size <= 0) return "Error: -size debe ser positivo";

    // Convertir unidad a bytes
    std::string unitLow = unit;
    std::transform(unitLow.begin(), unitLow.end(), unitLow.begin(), ::tolower);

    int sizeBytes = size;
    if      (unitLow == "k") sizeBytes = size * 1024;
    else if (unitLow == "m") sizeBytes = size * 1024 * 1024;
    else if (unitLow == "b" || unitLow.empty()) sizeBytes = size;

    // Tipo de partición
    std::string typeLow = type;
    std::transform(typeLow.begin(), typeLow.end(), typeLow.begin(), ::tolower);

    char partFit = 'F';
    std::string fitLow = fit;
    std::transform(fitLow.begin(), fitLow.end(), fitLow.begin(), ::tolower);
    if      (fitLow == "bf") partFit = 'B';
    else if (fitLow == "wf") partFit = 'W';
    else if (fitLow == "ff" || fitLow.empty()) partFit = 'F';

    std::string expandedPath = expandPath(path);

    if (typeLow == "l") {
        return createLogical(expandedPath, sizeBytes, partFit, name);
    }

    char partType = 'P';
    if (typeLow == "e") partType = 'E';

    return createPrimaryOrExtended(expandedPath, sizeBytes, partType, partFit, name);
}

} // namespace CommandFdisk

#endif