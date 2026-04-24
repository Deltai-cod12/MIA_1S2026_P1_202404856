#ifndef MKDIR_H
#define MKDIR_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <ctime>

#include "../session/session.h"
#include "../commands/mount.h"
#include "../models/structs.h"

namespace CommandMkdir {

// ===================== HELPERS =====================

inline std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> tokens;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/'))
        if (!item.empty()) tokens.push_back(item);
    return tokens;
}

inline int findFreeInode(std::fstream& file, Superblock& sb) {
    file.seekg(sb.s_bm_inode_start);
    for (int i = 0; i < sb.s_inodes_count; i++) {
        char bit; file.read(&bit, 1);
        if (bit == '0') return i;
    }
    return -1;
}

inline int findFreeBlock(std::fstream& file, Superblock& sb) {
    file.seekg(sb.s_bm_block_start);
    for (int i = 0; i < sb.s_blocks_count; i++) {
        char bit; file.read(&bit, 1);
        if (bit == '0') return i;
    }
    return -1;
}

inline Inode readInode(std::fstream& file, Superblock& sb, int index) {
    Inode inode{};
    file.seekg(sb.s_inode_start + index * sizeof(Inode));
    file.read((char*)&inode, sizeof(Inode));
    return inode;
}

inline void writeInode(std::fstream& file, Superblock& sb, int index, Inode& inode) {
    file.seekp(sb.s_inode_start + index * sizeof(Inode));
    file.write((char*)&inode, sizeof(Inode));
}

inline FolderBlock readFolderBlock(std::fstream& file, Superblock& sb, int blockIndex) {
    FolderBlock block{};
    file.seekg(sb.s_block_start + blockIndex * sizeof(FolderBlock));
    file.read((char*)&block, sizeof(FolderBlock));
    return block;
}

inline void writeFolderBlock(std::fstream& file, Superblock& sb, int blockIndex, FolderBlock& block) {
    file.seekp(sb.s_block_start + blockIndex * sizeof(FolderBlock));
    file.write((char*)&block, sizeof(FolderBlock));
}

// ===================== FIX CRÍTICO =====================
inline int findInDir(std::fstream& file, Superblock& sb, int dirInodeIndex, const std::string& name) {
    Inode dirInode = readInode(file, sb, dirInodeIndex);

    for (int b = 0; b < 12; b++) {
        if (dirInode.i_block[b] == -1) continue;

        FolderBlock block = readFolderBlock(file, sb, dirInode.i_block[b]);

        for (int i = 0; i < 4; i++) {
            if (block.b_content[i].b_inodo == -1) continue;

            std::string entryName(block.b_content[i].b_name);

            if (entryName == name)
                return block.b_content[i].b_inodo;
        }
    }
    return -1;
}

// ===================== AGREGAR ENTRADA =====================
inline bool addEntryToDir(std::fstream& file, Superblock& sb,
                          int dirInodeIndex, const std::string& name, int newInodeIndex) {

    Inode dirInode = readInode(file, sb, dirInodeIndex);

    for (int b = 0; b < 12; b++) {
        if (dirInode.i_block[b] == -1) continue;

        FolderBlock block = readFolderBlock(file, sb, dirInode.i_block[b]);

        for (int i = 0; i < 4; i++) {
            if (block.b_content[i].b_inodo == -1) {
                strncpy(block.b_content[i].b_name, name.c_str(), 11);
                block.b_content[i].b_name[11] = '\0';
                block.b_content[i].b_inodo = newInodeIndex;

                writeFolderBlock(file, sb, dirInode.i_block[b], block);

                dirInode.i_mtime = time(nullptr);
                writeInode(file, sb, dirInodeIndex, dirInode);
                return true;
            }
        }
    }

    int newBlockIndex = findFreeBlock(file, sb);
    if (newBlockIndex == -1) return false;

    int freeSlot = -1;
    for (int b = 0; b < 12; b++) {
        if (dirInode.i_block[b] == -1) {
            freeSlot = b;
            break;
        }
    }

    if (freeSlot == -1) return false;

    FolderBlock newBlock{};
    strncpy(newBlock.b_content[0].b_name, name.c_str(), 11);
    newBlock.b_content[0].b_name[11] = '\0';
    newBlock.b_content[0].b_inodo = newInodeIndex;

    writeFolderBlock(file, sb, newBlockIndex, newBlock);

    char used = '1';
    file.seekp(sb.s_bm_block_start + newBlockIndex);
    file.write(&used, 1);

    dirInode.i_block[freeSlot] = newBlockIndex;
    writeInode(file, sb, dirInodeIndex, dirInode);

    sb.s_free_blocks_count--;

    return true;
}

// ===================== CREAR DIRECTORIO =====================
inline int createDirectory(std::fstream& file, Superblock& sb,
                           const std::string& name, int parentInodeIndex) {

    int newInodeIndex = findFreeInode(file, sb);
    if (newInodeIndex == -1) return -1;

    int newBlockIndex = findFreeBlock(file, sb);
    if (newBlockIndex == -1) return -1;

    FolderBlock newBlock{};

    for (int i = 0; i < 4; i++) {
        newBlock.b_content[i].b_inodo = -1;
        memset(newBlock.b_content[i].b_name, 0, 12);
    }    
    strcpy(newBlock.b_content[0].b_name, ".");
    newBlock.b_content[0].b_inodo = newInodeIndex;

    strcpy(newBlock.b_content[1].b_name, "..");
    newBlock.b_content[1].b_inodo = parentInodeIndex;

    writeFolderBlock(file, sb, newBlockIndex, newBlock);

    Inode newInode{};
    newInode.i_uid = 1;
    newInode.i_gid = 1;
    newInode.i_type = '1';
    newInode.i_block[0] = newBlockIndex;
    strncpy(newInode.i_perm, "664", 3);

    writeInode(file, sb, newInodeIndex, newInode);

    char used = '1';
    file.seekp(sb.s_bm_inode_start + newInodeIndex);
    file.write(&used, 1);

    file.seekp(sb.s_bm_block_start + newBlockIndex);
    file.write(&used, 1);

    sb.s_free_inodes_count--;
    sb.s_free_blocks_count--;

    if (!addEntryToDir(file, sb, parentInodeIndex, name, newInodeIndex))
        return -1;

    return newInodeIndex;
}

// ===================== EJECUCIÓN =====================
inline std::string execute(const std::string& path, bool createParents) {

    if (!currentSession.active)
        return "Error: no hay sesion activa";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(currentSession.id, partition))
        return "Error: particion no montada";

    std::fstream file(partition.path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco";

    Superblock sb{};
    file.seekg(partition.start);
    file.read((char*)&sb, sizeof(Superblock));

    std::vector<std::string> folders = splitPath(path);
    if (folders.empty()) {
        file.close();
        return "Error: ruta invalida";
    }

    int currentInodeIndex = 0;

    for (int i = 0; i < (int)folders.size() - 1; i++) {

        int found = findInDir(file, sb, currentInodeIndex, folders[i]);

        if (found == -1) {
            if (!createParents) {
                file.close();
                return "Error: directorio padre '" + folders[i] + "' no existe";
            }

            found = createDirectory(file, sb, folders[i], currentInodeIndex);

            if (found == -1) {
                file.close();
                return "Error: no se pudo crear '" + folders[i] + "'";
            }
        } else {
            Inode inode = readInode(file, sb, found);
            if (inode.i_type != '1') {
                file.close();
                return "Error: '" + folders[i] + "' no es un directorio";
            }
        }

        if (found < 0) {
            file.close();
            return "Error interno: inodo invalido";
        }

        currentInodeIndex = found;
    }

    std::string newName = folders.back();

    if (findInDir(file, sb, currentInodeIndex, newName) != -1) {
        file.close();
        return "Error: '" + newName + "' ya existe";
    }

    if (createDirectory(file, sb, newName, currentInodeIndex) == -1) {
        file.close();
        return "Error: no hay espacio disponible";
    }

    file.seekp(partition.start);
    file.write((char*)&sb, sizeof(Superblock));

    file.close();
    return "Directorio creado correctamente";
}

// ===================== PARSER =====================
inline std::string executeFromLine(const std::string& commandLine) {

    std::string path;
    bool createParents = false;

    size_t pathPos = commandLine.find("-path=");
    if (pathPos == std::string::npos)
        pathPos = commandLine.find("-Path=");

    if (pathPos != std::string::npos) {
        size_t valStart = pathPos + 6;

        if (commandLine[valStart] == '"') {
            size_t end = commandLine.find('"', valStart + 1);
            path = commandLine.substr(valStart + 1, end - valStart - 1);
        } else {
            size_t end = commandLine.find(' ', valStart);
            if (end == std::string::npos) end = commandLine.size();
            path = commandLine.substr(valStart, end - valStart);
        }
    }

    std::string lower = commandLine;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    std::stringstream ss(commandLine);
    std::string token;

    while (ss >> token) {
        std::string t = token;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);

        if (t == "-p") {
            createParents = true;
        }
    }

    if (path.empty())
        return "Error: mkdir requiere -path";

    return execute(path, createParents);
}

} // namespace

#endif