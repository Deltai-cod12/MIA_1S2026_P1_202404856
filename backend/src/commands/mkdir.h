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

//  Helpers 

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
    Inode inode;
    file.seekg(sb.s_inode_start + index * sizeof(Inode));
    file.read((char*)&inode, sizeof(Inode));
    return inode;
}

inline void writeInode(std::fstream& file, Superblock& sb, int index, Inode& inode) {
    file.seekp(sb.s_inode_start + index * sizeof(Inode));
    file.write((char*)&inode, sizeof(Inode));
}

inline FolderBlock readFolderBlock(std::fstream& file, Superblock& sb, int blockIndex) {
    FolderBlock block;
    file.seekg(sb.s_block_start + blockIndex * sizeof(FolderBlock));
    file.read((char*)&block, sizeof(FolderBlock));
    return block;
}

inline void writeFolderBlock(std::fstream& file, Superblock& sb, int blockIndex, FolderBlock& block) {
    file.seekp(sb.s_block_start + blockIndex * sizeof(FolderBlock));
    file.write((char*)&block, sizeof(FolderBlock));
}


   // Busca un nombre en un directorio (por su inodo).
   // Retorna el índice de inodo si lo encuentra, -1 si no.

inline int findInDir(std::fstream& file, Superblock& sb, int dirInodeIndex, const std::string& name) {
    Inode dirInode = readInode(file, sb, dirInodeIndex);

    for (int b = 0; b < 12; b++) {
        if (dirInode.i_block[b] == -1) continue;
        FolderBlock block = readFolderBlock(file, sb, dirInode.i_block[b]);
        for (int i = 0; i < 4; i++) {
            if (block.b_content[i].b_inodo == -1) continue;

            //Comparar máximo 11 chars (b_name tiene 12 bytes: 11 útiles + \0)
            if (strncmp(block.b_content[i].b_name, name.c_str(), 11) == 0)
                return block.b_content[i].b_inodo;
        }
    }
    return -1;
}

    // Agrega una entrada (nombre va inodo) a un directorio.
    // Si los bloques actuales están llenos, asigna uno nuevo.
    // Retorna true si tuvo éxito.

inline bool addEntryToDir(std::fstream& file, Superblock& sb,
                          int dirInodeIndex, const std::string& name, int newInodeIndex) {
    Inode dirInode = readInode(file, sb, dirInodeIndex);

    // Busca slot libre en bloques ya asignados
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

    // No hay slot: necesita un bloque nuevo para el directorio padre
    int newBlockIndex = findFreeBlock(file, sb);
    if (newBlockIndex == -1) return false;

    int freeSlot = -1;
    for (int b = 0; b < 12; b++) {
        if (dirInode.i_block[b] == -1) { freeSlot = b; break; }
    }
    if (freeSlot == -1) return false;  // sin apuntadores directos disponibles

    FolderBlock newBlock;
    strncpy(newBlock.b_content[0].b_name, name.c_str(), 11);
    newBlock.b_content[0].b_name[11] = '\0';
    newBlock.b_content[0].b_inodo = newInodeIndex;
    // los otros 3 quedan en -1 por el constructor

    writeFolderBlock(file, sb, newBlockIndex, newBlock);

    char used = '1';
    file.seekp(sb.s_bm_block_start + newBlockIndex);
    file.write(&used, 1);

    dirInode.i_block[freeSlot] = newBlockIndex;
    dirInode.i_mtime = time(nullptr);
    writeInode(file, sb, dirInodeIndex, dirInode);

    sb.s_free_blocks_count--;

    return true;
}

   // Crea un directorio dentro de parentInodeIndex.
   // Retorna el índice del nuevo inodo, o -1 si falló.

inline int createDirectory(std::fstream& file, Superblock& sb,
                           const std::string& name, int parentInodeIndex) {
    int newInodeIndex = findFreeInode(file, sb);
    if (newInodeIndex == -1) return -1;

    int newBlockIndex = findFreeBlock(file, sb);
    if (newBlockIndex == -1) return -1;

    // Bloque con . y ..
    FolderBlock newBlock;
    strcpy(newBlock.b_content[0].b_name, ".");
    newBlock.b_content[0].b_inodo = newInodeIndex;
    strcpy(newBlock.b_content[1].b_name, "..");
    newBlock.b_content[1].b_inodo = parentInodeIndex;
    // [2] y [3] quedan en -1 por constructor

    writeFolderBlock(file, sb, newBlockIndex, newBlock);

    // Inodo del nuevo directorio
    Inode newInode;
    newInode.i_uid = 1;
    newInode.i_gid = 1;
    newInode.i_size = 0;
    newInode.i_type = '1';         
    newInode.i_block[0] = newBlockIndex;
    strncpy(newInode.i_perm, "664", 3);

    writeInode(file, sb, newInodeIndex, newInode);

    // Marcar bitmaps
    char used = '1';
    file.seekp(sb.s_bm_inode_start + newInodeIndex);
    file.write(&used, 1);
    file.seekp(sb.s_bm_block_start + newBlockIndex);
    file.write(&used, 1);

    sb.s_free_inodes_count--;
    sb.s_free_blocks_count--;

    // Agregar entrada al directorio padre
    if (!addEntryToDir(file, sb, parentInodeIndex, name, newInodeIndex))
        return -1;

    return newInodeIndex;
}

//  Comando principal 

inline std::string execute(const std::string& path, bool createParents) {

    if (!currentSession.active)
        return "Error: no hay sesion activa";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(currentSession.id, partition))
        return "Error: particion no montada";

    std::fstream file(partition.path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partition.start);
    file.read((char*)&sb, sizeof(Superblock));

    std::vector<std::string> folders = splitPath(path);
    if (folders.empty()) {
        file.close();
        return "Error: ruta invalida";
    }

    //  Navegar hasta el directorio padre 
    int currentInodeIndex = 0;  // empieza en raíz

    for (int i = 0; i < (int)folders.size() - 1; i++) {
        int found = findInDir(file, sb, currentInodeIndex, folders[i]);

        if (found == -1) {
            if (!createParents) {
                file.close();
                return "Error: directorio padre '" + folders[i] +
                       "' no existe. Use -p para crearlo automaticamente";
            }
            // Crear directorio intermedio con -p
            found = createDirectory(file, sb, folders[i], currentInodeIndex);
            if (found == -1) {
                file.close();
                return "Error: sin espacio para crear directorio intermedio '" + folders[i] + "'";
            }
        } else {
            // Verificar que sea carpeta
            Inode inode = readInode(file, sb, found);
            if (inode.i_type != '1') {
                file.close();
                return "Error: '" + folders[i] + "' no es un directorio";
            }
        }
        currentInodeIndex = found;
    }

    //  Crear el directorio final 
    const std::string& newName = folders.back();

    if (findInDir(file, sb, currentInodeIndex, newName) != -1) {
        file.close();
        return "Error: '" + newName + "' ya existe";
    }

    if (createDirectory(file, sb, newName, currentInodeIndex) == -1) {
        file.close();
        return "Error: no hay espacio disponible en el disco";
    }

    // Guardar superbloque actualizado
    file.seekp(partition.start);
    file.write((char*)&sb, sizeof(Superblock));

    file.close();
    return "Directorio '" + path + "' creado correctamente";
}

//  Parser 

inline std::string executeFromLine(const std::string& commandLine) {
    std::string path;
    bool createParents = false;

    // Extraer -path= manejando comillas con espacios
    size_t pathPos = commandLine.find("-path=");
    if (pathPos == std::string::npos)
        pathPos = commandLine.find("-Path=");

    if (pathPos != std::string::npos) {
        size_t valStart = pathPos + 6;
        if (valStart < commandLine.size() && commandLine[valStart] == '"') {
            // Ruta entre comillas (puede tener espacios)
            size_t closeQuote = commandLine.find('"', valStart + 1);
            if (closeQuote != std::string::npos)
                path = commandLine.substr(valStart + 1, closeQuote - valStart - 1);
        } else {
            // Ruta sin comillas — termina en espacio
            size_t valEnd = commandLine.find(' ', valStart);
            if (valEnd == std::string::npos) valEnd = commandLine.size();
            path = commandLine.substr(valStart, valEnd - valStart);
        }
    }

    // Detectar flag -p
    std::string lower = commandLine;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find(" -p") != std::string::npos || lower.find("\t-p") != std::string::npos)
        createParents = true;

    if (path.empty())
        return "Error: mkdir requiere -path";

    return execute(path, createParents);
}

}  // namespace CommandMkdir

#endif