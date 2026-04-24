#ifndef EXT_UTILS_H
#define EXT_UTILS_H

/**
 * ext_utils.h
 * Funciones de bajo nivel para EXT2/EXT3.
 */

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <ctime>
#include <algorithm>
#include "../models/structs.h"
#include "mount.h"
#include "journaling.h"
#include "../session/session.h"

namespace ExtUtils {

    // Superbloque
    inline Superblock readSuperblock(std::fstream& f, int partStart) {
        Superblock sb;
        f.seekg(partStart, std::ios::beg);
        f.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
        return sb;
    }

    inline void writeSuperblock(std::fstream& f, int partStart, const Superblock& sb) {
        f.seekp(partStart, std::ios::beg);
        f.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
    }

    // Inodos — mismos offsets que mkdir.h
    // mkdir usa: sb.s_inode_start + index * sizeof(Inode)
    inline Inode readInode(std::fstream& f, const Superblock& sb, int idx) {
        Inode in;
        f.seekg(sb.s_inode_start + idx * static_cast<int>(sizeof(Inode)), std::ios::beg);
        f.read(reinterpret_cast<char*>(&in), sizeof(Inode));
        return in;
    }

    inline void writeInode(std::fstream& f, const Superblock& sb, int idx, const Inode& in) {
        f.seekp(sb.s_inode_start + idx * static_cast<int>(sizeof(Inode)), std::ios::beg);
        f.write(reinterpret_cast<const char*>(&in), sizeof(Inode));
    }

    // Bloques de carpeta — mismos offsets que mkdir.h
    // mkdir usa: sb.s_block_start + blockIndex * sizeof(FolderBlock)
    // sizeof(FolderBlock) = 64 = sb.s_block_size
    inline FolderBlock readFolderBlock(std::fstream& f, const Superblock& sb, int idx) {
        FolderBlock fb;
        f.seekg(sb.s_block_start + idx * static_cast<int>(sizeof(FolderBlock)), std::ios::beg);
        f.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
        return fb;
    }

    inline void writeFolderBlock(std::fstream& f, const Superblock& sb, int idx,
                                  const FolderBlock& fb) {
        f.seekp(sb.s_block_start + idx * static_cast<int>(sizeof(FolderBlock)), std::ios::beg);
        f.write(reinterpret_cast<const char*>(&fb), sizeof(FolderBlock));
    }

    // Bloques de archivo
    inline FileBlock readFileBlock(std::fstream& f, const Superblock& sb, int idx) {
        FileBlock fb;
        f.seekg(sb.s_block_start + idx * static_cast<int>(sizeof(FileBlock)), std::ios::beg);
        f.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
        return fb;
    }

    inline void writeFileBlock(std::fstream& f, const Superblock& sb, int idx,
                                const FileBlock& fb) {
        f.seekp(sb.s_block_start + idx * static_cast<int>(sizeof(FileBlock)), std::ios::beg);
        f.write(reinterpret_cast<const char*>(&fb), sizeof(FileBlock));
    }

    // Bitmaps
    inline int allocateBitmap(std::fstream& f, int bitmapStart, int count) {
        for (int i = 0; i < count; i++) {
            char bit;
            f.seekg(bitmapStart + i, std::ios::beg);
            f.read(&bit, 1);
            if (bit == '0') {
                char used = '1';
                f.seekp(bitmapStart + i, std::ios::beg);
                f.write(&used, 1);
                return i;
            }
        }
        return -1;
    }

    inline void freeBitmap(std::fstream& f, int bitmapStart, int idx) {
        char free_ = '0';
        f.seekp(bitmapStart + idx, std::ios::beg);
        f.write(&free_, 1);
    }

    // BÚSQUEDA DE NOMBRES — COMPATIBILIDAD CON mkdir.h
    //
    // mkdir::addEntryToDir escribe: strncpy(b_name, name.c_str(), 11)
    // Esto limita el nombre a 11 chars útiles + '\0'.
    // Por tanto la búsqueda DEBE usar strncmp con 11 chars máximo,
    // igual que mkdir::findInDir: strncmp(b_name, name.c_str(), 11)
    //
    // Sin esto, "archivo1.txt" (12 chars) se guarda como "archivo1.tx"
    // y la búsqueda por "archivo1.txt" falla.
    inline int findInDir(std::fstream& f, const Superblock& sb,
                          int dirInodeNum, const std::string& name) {
        Inode dir = readInode(f, sb, dirInodeNum);
        // Truncar el nombre de búsqueda a 11 chars (límite de mkdir)
        std::string searchName = (name.size() > 11) ? name.substr(0, 11) : name;

        for (int i = 0; i < 12; i++) {
            if (dir.i_block[i] == -1) break;
            FolderBlock fb = readFolderBlock(f, sb, dir.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                // Usar strncmp con 11 chars, igual que mkdir::findInDir
                if (strncmp(fb.b_content[j].b_name, searchName.c_str(), 11) == 0)
                    return fb.b_content[j].b_inodo;
            }
        }
        return -1;
    }

    inline int traversePath(std::fstream& f, const Superblock& sb,
                             const std::string& path) {
        if (path == "/" || path.empty()) return 0;
        std::istringstream iss(path);
        std::string token;
        int cur = 0;
        while (std::getline(iss, token, '/')) {
            if (token.empty()) continue;
            cur = findInDir(f, sb, cur, token);
            if (cur == -1) return -1;
        }
        return cur;
    }

    inline void splitPath(const std::string& path,
                           std::string& parent, std::string& child) {
        size_t pos = path.rfind('/');
        if (pos == std::string::npos || pos == 0) {
            parent = "/";
            child  = (pos == 0) ? path.substr(1) : path;
        } else {
            parent = path.substr(0, pos);
            child  = path.substr(pos + 1);
        }
    }

    // Agregar entrada a directorio
    // Usa strncpy con 11 chars, igual que mkdir::addEntryToDir
    inline bool addEntryToDir(std::fstream& f, Superblock& sb, int partStart,
                               int dirInodeNum, const std::string& name, int childInode) {
        Inode dir = readInode(f, sb, dirInodeNum);
        for (int i = 0; i < 12; i++) {
            FolderBlock fb;
            int blockIdx;
            if (dir.i_block[i] == -1) {
                blockIdx = allocateBitmap(f, sb.s_bm_block_start, sb.s_blocks_count);
                if (blockIdx == -1) return false;
                sb.s_free_blocks_count--;
                dir.i_block[i] = blockIdx;
                // Inicializar bloque nuevo con -1
                FolderBlock emptyFb;
                writeFolderBlock(f, sb, blockIdx, emptyFb);
            } else {
                blockIdx = dir.i_block[i];
                fb = readFolderBlock(f, sb, blockIdx);
            }
            // Re-leer el bloque (puede ser nuevo o existente)
            fb = readFolderBlock(f, sb, blockIdx);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) {
                    // Usar strncpy con 11 chars igual que mkdir
                    memset(fb.b_content[j].b_name, 0, 12);
                    strncpy(fb.b_content[j].b_name, name.c_str(), 11);
                    fb.b_content[j].b_inodo = childInode;
                    writeFolderBlock(f, sb, blockIdx, fb);
                    dir.i_mtime = time(nullptr);
                    writeInode(f, sb, dirInodeNum, dir);
                    writeSuperblock(f, partStart, sb);
                    return true;
                }
            }
        }
        return false;
    }

    inline bool removeEntryFromDir(std::fstream& f, const Superblock& sb,
                                    int dirInodeNum, const std::string& name) {
        std::string searchName = (name.size() > 11) ? name.substr(0, 11) : name;
        Inode dir = readInode(f, sb, dirInodeNum);
        for (int i = 0; i < 12; i++) {
            if (dir.i_block[i] == -1) break;
            FolderBlock fb = readFolderBlock(f, sb, dir.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo != -1 &&
                    strncmp(fb.b_content[j].b_name, searchName.c_str(), 11) == 0) {
                    memset(fb.b_content[j].b_name, 0, 12);
                    fb.b_content[j].b_inodo = -1;
                    writeFolderBlock(f, sb, dir.i_block[i], fb);
                    return true;
                }
            }
        }
        return false;
    }

    // Contenido de archivos
    inline std::string readFileContent(std::fstream& f, const Superblock& sb,
                                        const Inode& inode) {
        std::string content;
        // Bloques directos
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) break;
            FileBlock fb = readFileBlock(f, sb, inode.i_block[i]);
            // Usar strnlen para no incluir bytes nulos de relleno
            int len = strnlen(fb.b_content, sizeof(fb.b_content));
            content.append(fb.b_content, len);
        }
        // Bloque indirecto simple
        if (inode.i_block[12] != -1) {
            PointerBlock pb;
            f.seekg(sb.s_block_start + inode.i_block[12] * static_cast<int>(sizeof(PointerBlock)),
                    std::ios::beg);
            f.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
            for (int i = 0; i < 16; i++) {
                if (pb.b_pointers[i] == -1) break;
                FileBlock fb = readFileBlock(f, sb, pb.b_pointers[i]);
                int len = strnlen(fb.b_content, sizeof(fb.b_content));
                content.append(fb.b_content, len);
            }
        }
        // Respetar i_size si está seteado
        if (inode.i_size > 0 && static_cast<int>(content.size()) > inode.i_size)
            content.resize(inode.i_size);
        return content;
    }

    inline bool writeFileContent(std::fstream& f, Superblock& sb, int partStart,
                                  int inodeNum, const std::string& content) {
        Inode inode = readInode(f, sb, inodeNum);
        // Liberar bloques directos anteriores
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] != -1) {
                freeBitmap(f, sb.s_bm_block_start, inode.i_block[i]);
                inode.i_block[i] = -1;
                sb.s_free_blocks_count++;
            }
        }
        int bs = static_cast<int>(sizeof(FileBlock)); // 64
        int needed = (static_cast<int>(content.size()) + bs - 1) / bs;
        if (needed > 12) needed = 12;
        for (int i = 0; i < needed; i++) {
            int bn = allocateBitmap(f, sb.s_bm_block_start, sb.s_blocks_count);
            if (bn == -1) return false;
            sb.s_free_blocks_count--;
            inode.i_block[i] = bn;
            FileBlock fb;
            int off = i * bs;
            int tc  = std::min(bs, static_cast<int>(content.size()) - off);
            if (tc > 0) memcpy(fb.b_content, content.c_str() + off, tc);
            writeFileBlock(f, sb, bn, fb);
        }
        inode.i_size  = static_cast<int>(content.size());
        inode.i_mtime = time(nullptr);
        writeInode(f, sb, inodeNum, inode);
        writeSuperblock(f, partStart, sb);
        return true;
    }

    // Permisos
    inline bool checkPermission(const Inode& inode, int currentUid,
                                  int currentGid, char need) {
        if (currentUid == 1) return true; // root puede todo
        auto d = [](char c) -> int {
            return (c >= '0' && c <= '7') ? (c - '0') : 0;
        };
        int pU = d(inode.i_perm[0]);
        int pG = d(inode.i_perm[1]);
        int pO = d(inode.i_perm[2]);
        int rel;
        if      (currentUid == inode.i_uid) rel = pU;
        else if (currentGid == inode.i_gid) rel = pG;
        else                                rel = pO;
        if (need == 'r') return (rel & 4) != 0;
        if (need == 'w') return (rel & 2) != 0;
        if (need == 'x') return (rel & 1) != 0;
        return false;
    }

    // Liberar inodo y sus bloques
    inline void freeInode(std::fstream& f, Superblock& sb, int partStart, int inodeNum) {
        Inode inode = readInode(f, sb, inodeNum);
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] != -1) {
                freeBitmap(f, sb.s_bm_block_start, inode.i_block[i]);
                sb.s_free_blocks_count++;
            }
        }
        if (inode.i_block[12] != -1) {
            PointerBlock pb;
            f.seekg(sb.s_block_start + inode.i_block[12] * static_cast<int>(sizeof(PointerBlock)),
                    std::ios::beg);
            f.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
            for (int i = 0; i < 16; i++) {
                if (pb.b_pointers[i] != -1) {
                    freeBitmap(f, sb.s_bm_block_start, pb.b_pointers[i]);
                    sb.s_free_blocks_count++;
                }
            }
            freeBitmap(f, sb.s_bm_block_start, inode.i_block[12]);
            sb.s_free_blocks_count++;
        }
        freeBitmap(f, sb.s_bm_inode_start, inodeNum);
        sb.s_free_inodes_count++;
        writeSuperblock(f, partStart, sb);
    }

    // Obtener UID de usuario leyendo users.txt (inodo 1)
    // Formato P1: id,U,grupo,nombre,pass
    inline int getUserUID(std::fstream& f, const Superblock& sb,
                           const std::string& username) {
        Inode usersInode = readInode(f, sb, 1);
        std::string content = readFileContent(f, sb, usersInode);
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            // Limpiar \r si existe (Windows line endings)
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ls(line);
            std::string idStr, tipo, grp, nombre;
            std::getline(ls, idStr, ',');
            std::getline(ls, tipo,  ',');
            if (tipo != "U") continue;
            std::getline(ls, grp,   ',');
            std::getline(ls, nombre,',');
            if (nombre == username) {
                try { return std::stoi(idStr); } catch(...) {}
            }
        }
        return -1;
    }

    // Abrir la partición de la sesión activa
    inline bool openSession(std::fstream& file, Superblock& sb,
                             CommandMount::MountedPartition& part) {
        if (!currentSession.active) return false;
        if (!CommandMount::getMountedPartition(currentSession.id, part)) return false;
        file.open(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return false;
        sb = readSuperblock(file, part.start);
        return true;
    }

    // UID del usuario de la sesión (root siempre = 1)
    inline int sessionUID(std::fstream& f, const Superblock& sb) {
        if (!currentSession.active) return -1;
        if (currentSession.user == "root") return 1;
        return getUserUID(f, sb, currentSession.user);
    }

    // GID del grupo de la sesión
    inline int sessionGID(std::fstream& f, const Superblock& sb) {
        if (!currentSession.active) return -1;
        Inode ui = readInode(f, sb, 1);
        std::string content = readFileContent(f, sb, ui);
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ls(line);
            std::string idStr, tipo, nombre;
            std::getline(ls, idStr, ',');
            std::getline(ls, tipo,  ',');
            std::getline(ls, nombre,',');
            if (tipo == "G" && nombre == currentSession.group) {
                try { return std::stoi(idStr); } catch(...) {}
            }
        }
        return -1;
    }

    // Registrar en journal
    inline void journalAdd(std::fstream& f, Superblock& sb, int partStart,
                            const std::string& op,
                            const std::string& path,
                            const std::string& content) {
        if (!currentSession.active) return;
        CommandJournaling::writeToDisk(f, sb, partStart, op, path, content);
        CommandJournaling::add(currentSession.id, op, path, content);
    }

} // namespace ExtUtils

#endif // EXT_UTILS_H