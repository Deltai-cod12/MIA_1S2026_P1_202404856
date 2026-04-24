#ifndef UNMOUNT_H
#define UNMOUNT_H

/**
 * unmount.h
 * Desmonta una partición usando su ID de montaje.
 */

#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include "../models/structs.h"
#include "mount.h"

namespace CommandUnmount {

inline std::string getParam(const std::string& line, const std::string& key) {
    std::string ll = line, lk = key;
    std::transform(ll.begin(), ll.end(), ll.begin(), ::tolower);
    std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
    size_t pos = ll.find(lk + "=");
    if (pos == std::string::npos) return "";
    pos += key.size() + 1;
    if (pos >= line.size()) return "";
    if (line[pos] == '"' || line[pos] == '\'') {
        char q = line[pos];
        size_t end = line.find(q, pos + 1);
        return line.substr(pos + 1,
            (end == std::string::npos ? line.size() : end) - pos - 1);
    }
    size_t end = line.find(' ', pos);
    return line.substr(pos, (end == std::string::npos ? line.size() : end) - pos);
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::string id = getParam(commandLine, "-id");
    if (id.empty()) return "Error: unmount requiere -id";

    // Verificar que existe la partición montada
    CommandMount::MountedPartition part;
    if (!CommandMount::getMountedPartition(id, part))
        return "Error: no existe particion montada con ID '" + id + "'";

    // Resetear correlativo en el MBR del disco
    std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
    if (file.is_open()) {
        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                std::string pname(mbr.mbr_partitions[i].part_name);
                if (pname == part.name) {
                    mbr.mbr_partitions[i].part_correlative = 0;
                    memset(mbr.mbr_partitions[i].part_id, 0, 4);
                    file.seekp(0, std::ios::beg);
                    file.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
                    break;
                }
            }
        }
        file.close();
    }

    // Quitar de la lista global usando CommandMount
    CommandMount::unmountPartition(id);

    std::ostringstream out;
    out << "\n=== UNMOUNT ===\n";
    out << "Particion desmontada exitosamente\n";
    out << "  ID       : " << id        << "\n";
    out << "  Disco    : " << part.path << "\n";
    out << "  Particion: " << part.name << "\n";
    return out.str();
}

} // namespace CommandUnmount

#endif // UNMOUNT_H