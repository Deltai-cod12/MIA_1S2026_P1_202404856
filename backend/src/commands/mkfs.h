#ifndef MKFS_H
#define MKFS_H

/**
 * mkfs.h — P2 (compatible con P1)
 * ----------------------------------------------------------
 * Nuevos parámetros:
 *   -fs=2fs   → EXT2 (comportamiento original del P1)
 *   -fs=3fs   → EXT3 (agrega área de journaling)
 *
 * Layout EXT2 (igual que P1):
 *   [SB][BitmapInodos(n)][BitmapBloques(3n)][Inodos(n)][Bloques(3n)]
 *
 * Layout EXT3:
 *   [SB][Journal(50×sizeof(Journal))][BitmapInodos(n)][BitmapBloques(3n)][Inodos(n)][Bloques(3n)]
 *
 * Usa CommandMount::getMountedPartition y MountedPartition.path/start/size
 * ----------------------------------------------------------
 */

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <vector>

#include "../models/structs.h"
#include "mount.h"
#include "journaling.h"

namespace CommandMkfs {

inline std::string toLowerCase(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

inline std::string getParam(const std::string& line, const std::string& key) {
    std::string lline = toLowerCase(line);
    std::string lkey  = toLowerCase(key);
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

// Escribe el sistema de archivos inicial (raíz + users.txt)
// Funciona igual para EXT2 y EXT3 ya que sb ya tiene los offsets correctos
inline void writeInitialFS(std::fstream& file, const Superblock& sb) {
    // -- Bitmap inodos --
    file.seekp(sb.s_bm_inode_start);
    for (int i = 0; i < sb.s_inodes_count; i++) {
        char bit = (i == 0 || i == 1) ? '1' : '0';
        file.write(&bit, 1);
    }

    // -- Bitmap bloques --
    file.seekp(sb.s_bm_block_start);
    for (int i = 0; i < sb.s_blocks_count; i++) {
        char bit = (i == 0 || i == 1) ? '1' : '0';
        file.write(&bit, 1);
    }

    // -- Inodo raíz (0) --
    Inode root;
    root.i_uid = 1; root.i_gid = 1; root.i_size = 0;
    root.i_atime = root.i_ctime = root.i_mtime = time(nullptr);
    root.i_type = '1';
    memcpy(root.i_perm, "775", 3);
    root.i_block[0] = 0;
    file.seekp(sb.s_inode_start);
    file.write(reinterpret_cast<const char*>(&root), sizeof(Inode));

    // -- Inodo users.txt (1) --
    std::string usersContent = "1,G,root\n1,U,root,root,123\n";
    Inode users;
    users.i_uid = 1; users.i_gid = 1;
    users.i_size = static_cast<int>(usersContent.size());
    users.i_atime = users.i_ctime = users.i_mtime = time(nullptr);
    users.i_type = '0';
    memcpy(users.i_perm, "664", 3);
    users.i_block[0] = 1;
    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<const char*>(&users), sizeof(Inode));

    // -- Bloque carpeta raíz (0) --
    FolderBlock rootBlock;
    strcpy(rootBlock.b_content[0].b_name, ".");
    rootBlock.b_content[0].b_inodo = 0;
    strcpy(rootBlock.b_content[1].b_name, "..");
    rootBlock.b_content[1].b_inodo = 0;
    strcpy(rootBlock.b_content[2].b_name, "users.txt");
    rootBlock.b_content[2].b_inodo = 1;
    rootBlock.b_content[3].b_inodo = -1;
    file.seekp(sb.s_block_start);
    file.write(reinterpret_cast<const char*>(&rootBlock), sizeof(FolderBlock));

    // -- Bloque users.txt (1) --
    FileBlock usersBlock;
    strncpy(usersBlock.b_content, usersContent.c_str(), 64);
    file.seekp(sb.s_block_start + sizeof(FolderBlock));
    file.write(reinterpret_cast<const char*>(&usersBlock), sizeof(FileBlock));
}

inline std::string execute(const std::string& id,
                            const std::string& type,
                            const std::string& fs) {
    if (id.empty()) return "Error: mkfs requiere el parametro -id";

    std::string formatType = toLowerCase(type);
    if (formatType.empty()) formatType = "full";
    if (formatType != "full" && formatType != "fast")
        return "Error: -type solo puede ser full o fast";

    // Determinar tipo de filesystem
    int fsType = 2; // EXT2 por defecto
    if (!fs.empty()) {
        std::string fsl = toLowerCase(fs);
        if      (fsl == "2fs") fsType = 2;
        else if (fsl == "3fs") fsType = 3;
        else return "Error: -fs acepta '2fs' o '3fs'";
    }

    // Buscar partición montada
    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(id, partition))
        return "Error: la particion con ID '" + id + "' no esta montada";

    std::fstream file(partition.path,
                      std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco '" + partition.path + "'";

    int partSize  = partition.size;
    int partStart = partition.start;

    // -- Formateo full: limpiar toda la partición --
    if (formatType == "full") {
        file.seekp(partStart);
        char zero = '\0';
        for (int i = 0; i < partSize; i++) file.write(&zero, 1);
    }

    // -- Calcular n (número de estructuras) --
    int denom = 4 + static_cast<int>(sizeof(Inode)) + 3 * static_cast<int>(sizeof(FileBlock));
    int n;

    if (fsType == 2) {
        // EXT2: igual que P1
        n = (partSize - static_cast<int>(sizeof(Superblock))) / denom;
    } else {
        // EXT3: descontar el área de journaling
        int journalArea = JOURNAL_MAX * static_cast<int>(sizeof(Journal));
        n = (partSize - static_cast<int>(sizeof(Superblock)) - journalArea) / denom;
    }

    if (n <= 0) {
        file.close();
        return "Error: particion demasiado pequena para crear el sistema de archivos";
    }

    // -- Construir Superbloque --
    Superblock sb;
    sb.s_filesystem_type   = fsType;
    sb.s_inodes_count      = n;
    sb.s_blocks_count      = 3 * n;
    sb.s_free_blocks_count = 3 * n - 2;
    sb.s_free_inodes_count = n - 2;
    sb.s_mtime             = time(nullptr);
    sb.s_umtime            = time(nullptr);
    sb.s_mnt_count         = 1;
    sb.s_magic             = 0xEF53;
    sb.s_inode_size        = static_cast<int>(sizeof(Inode));
    sb.s_block_size        = static_cast<int>(sizeof(FileBlock));
    sb.s_first_ino         = 2;
    sb.s_first_blo         = 2;
    sb.s_journal_count     = 0;

    int base = partStart + static_cast<int>(sizeof(Superblock));

    if (fsType == 3) {
        int journalArea      = JOURNAL_MAX * static_cast<int>(sizeof(Journal));
        sb.s_journal_start   = base;
        sb.s_bm_inode_start  = base + journalArea;
    } else {
        sb.s_journal_start   = 0;
        sb.s_bm_inode_start  = base;
    }

    sb.s_bm_block_start = sb.s_bm_inode_start + n;
    sb.s_inode_start    = sb.s_bm_block_start  + 3 * n;
    sb.s_block_start    = sb.s_inode_start      + n * static_cast<int>(sizeof(Inode));

    // -- Escribir Superbloque --
    file.seekp(partStart);
    file.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));

    // -- Si EXT3: inicializar área de journal con entradas vacías --
    if (fsType == 3) {
        Journal emptyJ;
        for (int i = 0; i < JOURNAL_MAX; i++) {
            int offset = sb.s_journal_start + i * static_cast<int>(sizeof(Journal));
            file.seekp(offset);
            file.write(reinterpret_cast<const char*>(&emptyJ), sizeof(Journal));
        }
    }

    // -- Escribir FS inicial (raíz + users.txt) --
    writeInitialFS(file, sb);
    file.close();

    // Limpiar journal en memoria
    CommandJournaling::clearFor(id);

    std::ostringstream result;
    result << "\n=== MKFS ===\n";
    result << "Sistema de archivos EXT" << fsType << " creado correctamente\n";
    result << "ID: "         << id             << "\n";
    result << "Disco: "      << partition.path << "\n";
    result << "Particion: "  << partition.name << "\n";
    result << "Tipo formato: "<< formatType     << "\n";
    result << "Inodos: "     << n              << "\n";
    result << "Bloques: "    << (3 * n)        << "\n";
    if (fsType == 3)
        result << "Journal: " << JOURNAL_MAX << " entradas disponibles\n";
    result << "Archivo users.txt creado\n";
    return result.str();
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::string id   = getParam(commandLine, "-id");
    std::string type = getParam(commandLine, "-type");
    std::string fs   = getParam(commandLine, "-fs");
    return execute(id, type, fs);
}

} // namespace CommandMkfs

#endif // MKFS_H