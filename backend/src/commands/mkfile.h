#ifndef MKFILE_H
#define MKFILE_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <ctime>

#include "../models/structs.h"
#include "../session/session.h"
#include "mount.h"
#include "mkdir.h"

namespace CommandMkfile {

inline std::string toLowerCase(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

inline bool writeFileContent(std::fstream& file, Superblock& sb,
                              Inode& inode, const std::string& data) {
    int totalChars = (int)data.size();
    int written = 0;
    int blockNum = 0;

    while (written < totalChars) {

        int freeBlock = CommandMkdir::findFreeBlock(file, sb);
        if (freeBlock == -1) return false;

        FileBlock fb;
        int chunkSize = std::min(64, totalChars - written);
        memset(fb.b_content, 0, 64);
        memcpy(fb.b_content, data.c_str() + written, chunkSize);

        file.seekp(sb.s_block_start + freeBlock * sizeof(FileBlock));
        file.write((char*)&fb, sizeof(FileBlock));

        char used = '1';
        file.seekp(sb.s_bm_block_start + freeBlock);
        file.write(&used, 1);
        sb.s_free_blocks_count--;

        if (blockNum < 12) {
            inode.i_block[blockNum] = freeBlock;

        } else if (blockNum < 12 + 16) {
            if (inode.i_block[12] == -1) {
                int ptrBlock = CommandMkdir::findFreeBlock(file, sb);
                if (ptrBlock == -1) return false;

                PointerBlock pb;
                file.seekp(sb.s_block_start + ptrBlock * sizeof(PointerBlock));
                file.write((char*)&pb, sizeof(PointerBlock));

                char u = '1';
                file.seekp(sb.s_bm_block_start + ptrBlock);
                file.write(&u, 1);
                sb.s_free_blocks_count--;

                inode.i_block[12] = ptrBlock;
            }

            PointerBlock pb;
            file.seekg(sb.s_block_start + inode.i_block[12] * sizeof(PointerBlock));
            file.read((char*)&pb, sizeof(PointerBlock));

            pb.b_pointers[blockNum - 12] = freeBlock;

            file.seekp(sb.s_block_start + inode.i_block[12] * sizeof(PointerBlock));
            file.write((char*)&pb, sizeof(PointerBlock));

        } else if (blockNum < 12 + 16 + 16 * 16) {
            if (inode.i_block[13] == -1) {
                int ptrBlock = CommandMkdir::findFreeBlock(file, sb);
                if (ptrBlock == -1) return false;

                PointerBlock pb;
                file.seekp(sb.s_block_start + ptrBlock * sizeof(PointerBlock));
                file.write((char*)&pb, sizeof(PointerBlock));

                char u = '1';
                file.seekp(sb.s_bm_block_start + ptrBlock);
                file.write(&u, 1);
                sb.s_free_blocks_count--;

                inode.i_block[13] = ptrBlock;
            }

            int offset = blockNum - 12 - 16;
            int outerIdx = offset / 16;
            int innerIdx = offset % 16;

            PointerBlock outerPb;
            file.seekg(sb.s_block_start + inode.i_block[13] * sizeof(PointerBlock));
            file.read((char*)&outerPb, sizeof(PointerBlock));

            if (outerPb.b_pointers[outerIdx] == -1) {
                int innerBlock = CommandMkdir::findFreeBlock(file, sb);
                if (innerBlock == -1) return false;

                PointerBlock innerPb;
                file.seekp(sb.s_block_start + innerBlock * sizeof(PointerBlock));
                file.write((char*)&innerPb, sizeof(PointerBlock));

                char u = '1';
                file.seekp(sb.s_bm_block_start + innerBlock);
                file.write(&u, 1);
                sb.s_free_blocks_count--;

                outerPb.b_pointers[outerIdx] = innerBlock;
                file.seekp(sb.s_block_start + inode.i_block[13] * sizeof(PointerBlock));
                file.write((char*)&outerPb, sizeof(PointerBlock));
            }

            PointerBlock innerPb;
            file.seekg(sb.s_block_start + outerPb.b_pointers[outerIdx] * sizeof(PointerBlock));
            file.read((char*)&innerPb, sizeof(PointerBlock));

            innerPb.b_pointers[innerIdx] = freeBlock;

            file.seekp(sb.s_block_start + outerPb.b_pointers[outerIdx] * sizeof(PointerBlock));
            file.write((char*)&innerPb, sizeof(PointerBlock));

        } else {
            return false;
        }

        written += chunkSize;
        blockNum++;
    }

    return true;
}

inline std::string execute(
    const std::string& path,
    int size,
    const std::string& cont,
    bool recursive
) {
    if (!currentSession.active)
        return "Error: no hay sesion activa";

    if (path.empty())
        return "Error: mkfile requiere -path";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(currentSession.id, partition))
        return "Error: particion no montada";

    std::fstream file(partition.path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partition.start);
    file.read((char*)&sb, sizeof(Superblock));

    // Navegar/crear directorio padre
    std::vector<std::string> parts = CommandMkdir::splitPath(path);
    if (parts.empty()) {
        file.close();
        return "Error: ruta invalida";
    }

    std::string fileName = parts.back();
    int parentInodeIndex = 0;

    for (int i = 0; i < (int)parts.size() - 1; i++) {
        int found = CommandMkdir::findInDir(file, sb, parentInodeIndex, parts[i]);
        if (found == -1) {
            if (!recursive) {
                file.close();
                return "Error: directorio '" + parts[i] +
                       "' no existe. Use -r para crearlo automaticamente";
            }
            found = CommandMkdir::createDirectory(file, sb, parts[i], parentInodeIndex);
            if (found == -1) {
                file.close();
                return "Error: no se pudo crear directorio '" + parts[i] + "'";
            }
        } else {
            Inode inode = CommandMkdir::readInode(file, sb, found);
            if (inode.i_type != '1') {
                file.close();
                return "Error: '" + parts[i] + "' no es un directorio";
            }
        }
        parentInodeIndex = found;
    }

    if (CommandMkdir::findInDir(file, sb, parentInodeIndex, fileName) != -1) {
        file.close();
        return "Error: el archivo '" + fileName + "' ya existe";
    }

    // ── Preparar contenido ────────────────────────────────────────────────────
    std::string data;

    if (!cont.empty()) {
        std::ifstream hostFile(cont);
        if (hostFile.is_open()) {
            // Leer contenido del archivo del host
            data.assign(
                std::istreambuf_iterator<char>(hostFile),
                std::istreambuf_iterator<char>()
            );
            hostFile.close();
        } else {
            // No guardar la ruta como contenido — retornar error
            file.close();
            return "Error: no se pudo abrir el archivo -cont='" + cont + "'";
        }
    } else if (size > 0) {
        // Secuencia 0-9 repetida
        data.clear();
        for (int i = 0; i < size; i++) {
            data += char('0' + (i % 10));
        }
    }
    // size == 0 y cont vacío → archivo vacío

    // Crear inodo
    int newInodeIndex = CommandMkdir::findFreeInode(file, sb);
    if (newInodeIndex == -1) {
        file.close();
        return "Error: no hay inodos libres";
    }

    Inode newInode;
    newInode.i_uid  = 1;
    newInode.i_gid  = 1;
    newInode.i_size = (int)data.size();
    newInode.i_type = '0';
    strncpy(newInode.i_perm, "664", 3);

    char used = '1';
    file.seekp(sb.s_bm_inode_start + newInodeIndex);
    file.write(&used, 1);
    sb.s_free_inodes_count--;

    if (!data.empty()) {
        if (!writeFileContent(file, sb, newInode, data)) {
            file.close();
            return "Error: no hay bloques suficientes para el archivo";
        }
    }

    CommandMkdir::writeInode(file, sb, newInodeIndex, newInode);

    if (!CommandMkdir::addEntryToDir(file, sb, parentInodeIndex, fileName, newInodeIndex)) {
        file.close();
        return "Error: no hay espacio en el directorio padre";
    }

    file.seekp(partition.start);
    file.write((char*)&sb, sizeof(Superblock));

    file.close();

    std::ostringstream result;
    result << "\n=== MKFILE ===\n";
    result << "Archivo creado correctamente\n";
    result << "Path:   " << path << "\n";
    result << "Inodo:  " << newInodeIndex << "\n";
    result << "Tamano: " << data.size() << " bytes\n";
    return result.str();
}

inline std::string executeFromLine(const std::string& commandLine) {
    std::string path;
    std::string cont;
    int size = 0;
    bool recursive = false;

    // Extraer -path= con soporte para comillas y espacios
    size_t pathPos = commandLine.find("-path=");
    if (pathPos != std::string::npos) {
        size_t valStart = pathPos + 6;
        if (valStart < commandLine.size() && commandLine[valStart] == '"') {
            size_t closeQuote = commandLine.find('"', valStart + 1);
            if (closeQuote != std::string::npos)
                path = commandLine.substr(valStart + 1, closeQuote - valStart - 1);
        } else {
            size_t valEnd = commandLine.find(' ', valStart);
            if (valEnd == std::string::npos) valEnd = commandLine.size();
            path = commandLine.substr(valStart, valEnd - valStart);
        }
    }

    // Parsear resto de parámetros con istringstream
    std::istringstream iss(commandLine);
    std::string token;
    iss >> token; // saltar "mkfile"

    while (iss >> token) {
        std::string lower = toLowerCase(token);

        if (lower.find("-size=") == 0) {
            size = std::stoi(token.substr(6));
            if (size < 0)
                return "Error: -size no puede ser negativo";
        } else if (lower.find("-cont=") == 0) {
            cont = token.substr(6);
        } else if (lower == "-r") {
            recursive = true;
        }
    }

    if (path.empty())
        return "Error: mkfile requiere -path";

    return execute(path, size, cont, recursive);
}

}  // namespace CommandMkfile

#endif