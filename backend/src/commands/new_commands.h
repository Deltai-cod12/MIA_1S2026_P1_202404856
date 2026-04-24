#ifndef NEW_COMMANDS_H
#define NEW_COMMANDS_H

/**
 * new_commands.h
 * Comandos: remove, rename, copy, move, find, chown, chmod, loss
 */

#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <ctime>
#include "../models/structs.h"
#include "mount.h"
#include "../session/session.h"
#include "ext_utils.h"
#include "journaling.h"


// Helpers compartidos

namespace NewCmdUtils {

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

// Detectar flag booleano como "-r" (sin valor =)
inline bool hasFlag(const std::string& line, const std::string& flag) {
    std::string ll = line;
    std::transform(ll.begin(), ll.end(), ll.begin(), ::tolower);
    std::string fl = flag;
    std::transform(fl.begin(), fl.end(), fl.begin(), ::tolower);
    size_t pos = 0;
    while ((pos = ll.find(fl, pos)) != std::string::npos) {
        bool beforeOk = (pos == 0 || ll[pos-1] == ' ' || ll[pos-1] == '\t');
        bool afterOk  = (pos + fl.size() >= ll.size() ||
                         ll[pos + fl.size()] == ' ' ||
                         ll[pos + fl.size()] == '\t' ||
                         ll[pos + fl.size()] == '\r' ||
                         ll[pos + fl.size()] == '\n');
        if (beforeOk && afterOk) return true;
        pos++;
    }
    return false;
}

// Abrir la partición de la sesión activa
inline bool openSession(std::fstream& file, Superblock& sb,
                         CommandMount::MountedPartition& part) {
    if (!currentSession.active) return false;
    if (!CommandMount::getMountedPartition(currentSession.id, part)) return false;
    file.open(part.path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return false;
    sb = ExtUtils::readSuperblock(file, part.start);
    return true;
}

// UID del usuario de la sesión actual (root=1)
inline int sessionUID(std::fstream& f, const Superblock& sb) {
    if (!currentSession.active) return -1;
    if (currentSession.user == "root") return 1;
    return ExtUtils::getUserUID(f, sb, currentSession.user);
}

// GID del grupo de la sesión actual
inline int sessionGID(std::fstream& f, const Superblock& sb) {
    if (!currentSession.active) return -1;
    Inode ui = ExtUtils::readInode(f, sb, 1);
    std::string content = ExtUtils::readFileContent(f, sb, ui);
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
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

} // namespace NewCmdUtils


// REMOVE — elimina archivo/carpeta con verificación de permisos
// Flag -r para carpetas no vacías

namespace CommandRemove {

inline bool removeRecursive(std::fstream& f, Superblock& sb, int partStart,
                              int inodeNum, int uid, int gid, bool recursive) {
    Inode inode = ExtUtils::readInode(f, sb, inodeNum);

    // Verificar permiso de escritura
    if (!ExtUtils::checkPermission(inode, uid, gid, 'w')) return false;

    if (inode.i_type == '1') {
        // Es carpeta: revisar si tiene hijos
        bool hasChildren = false;
        for (int i = 0; i < 12 && !hasChildren; i++) {
            if (inode.i_block[i] == -1) break;
            FolderBlock fb = ExtUtils::readFolderBlock(f, sb, inode.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string n(fb.b_content[j].b_name);
                if (n != "." && n != "..") { hasChildren = true; break; }
            }
        }

        // Sin -r y tiene hijos → error
        if (hasChildren && !recursive) return false;

        // Eliminar hijos recursivamente
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) break;
            FolderBlock fb = ExtUtils::readFolderBlock(f, sb, inode.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string name(fb.b_content[j].b_name);
                if (name == "." || name == "..") continue;
                int child = fb.b_content[j].b_inodo;
                if (!removeRecursive(f, sb, partStart, child, uid, gid, true))
                    return false;
                memset(fb.b_content[j].b_name, 0, 12);
                fb.b_content[j].b_inodo = -1;
                ExtUtils::writeFolderBlock(f, sb, inode.i_block[i], fb);
                // Releer inode en caso de que cambió por el freeInode de hijos
                inode = ExtUtils::readInode(f, sb, inodeNum);
            }
        }
    }

    ExtUtils::freeInode(f, sb, partStart, inodeNum);
    return true;
}

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string path = NewCmdUtils::getParam(commandLine, "-path");
    bool recursive   = NewCmdUtils::hasFlag(commandLine, "-r");
    if (path.empty()) return "Error: remove requiere -path";

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);
    int gid = NewCmdUtils::sessionGID(file, sb);

    std::string parentPath, name;
    ExtUtils::splitPath(path, parentPath, name);
    if (name.empty()) { file.close(); return "Error: ruta invalida"; }

    int parentInode = (parentPath == "/" || parentPath.empty())
                      ? 0 : ExtUtils::traversePath(file, sb, parentPath);
    if (parentInode == -1) { file.close(); return "Error: directorio padre no existe"; }

    int targetInode = ExtUtils::findInDir(file, sb, parentInode, name);
    if (targetInode == -1) { file.close(); return "Error: '" + path + "' no existe"; }

    // Si es carpeta no vacía sin -r → error descriptivo
    Inode ti = ExtUtils::readInode(file, sb, targetInode);
    if (ti.i_type == '1' && !recursive) {
        bool hasChildren = false;
        for (int i = 0; i < 12 && !hasChildren; i++) {
            if (ti.i_block[i] == -1) break;
            FolderBlock fb = ExtUtils::readFolderBlock(file, sb, ti.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string n(fb.b_content[j].b_name);
                if (n != "." && n != "..") { hasChildren = true; break; }
            }
        }
        if (hasChildren) {
            file.close();
            return "Error: '" + path + "' es una carpeta no vacia. Use -r para eliminar recursivamente";
        }
    }

    if (!removeRecursive(file, sb, part.start, targetInode, uid, gid, recursive)) {
        file.close();
        return "Error: sin permiso de escritura para eliminar '" + path + "'";
    }

    ExtUtils::removeEntryFromDir(file, sb, parentInode, name);
    ExtUtils::journalAdd(file, sb, part.start, "remove", path, "-");
    file.close();
    return "remove: '" + path + "' eliminado exitosamente";
}

} // namespace CommandRemove


// RENAME — cambia nombre con verificación de permisos

namespace CommandRename {

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string path    = NewCmdUtils::getParam(commandLine, "-path");
    std::string newName = NewCmdUtils::getParam(commandLine, "-name");
    if (path.empty())    return "Error: rename requiere -path";
    if (newName.empty()) return "Error: rename requiere -name";

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);
    int gid = NewCmdUtils::sessionGID(file, sb);

    std::string parentPath, oldName;
    ExtUtils::splitPath(path, parentPath, oldName);
    if (oldName.empty()) { file.close(); return "Error: ruta invalida"; }

    int parentInode = (parentPath == "/" || parentPath.empty())
                      ? 0 : ExtUtils::traversePath(file, sb, parentPath);
    if (parentInode == -1) { file.close(); return "Error: directorio padre no existe"; }

    int targetInode = ExtUtils::findInDir(file, sb, parentInode, oldName);
    if (targetInode == -1) { file.close(); return "Error: '" + path + "' no existe"; }

    Inode inode = ExtUtils::readInode(file, sb, targetInode);
    if (!ExtUtils::checkPermission(inode, uid, gid, 'w')) {
        file.close();
        return "Error: sin permiso de escritura sobre '" + path + "'";
    }

    // Verificar que el nuevo nombre no exista ya
    if (ExtUtils::findInDir(file, sb, parentInode, newName) != -1) {
        file.close();
        return "Error: ya existe '" + newName + "' en el mismo directorio";
    }

    // Cambiar nombre en la entrada del directorio padre
    Inode parentDir = ExtUtils::readInode(file, sb, parentInode);
    bool done = false;
    for (int i = 0; i < 12 && !done; i++) {
        if (parentDir.i_block[i] == -1) break;
        FolderBlock fb = ExtUtils::readFolderBlock(file, sb, parentDir.i_block[i]);
        for (int j = 0; j < 4 && !done; j++) {
            // Usar strncmp con 11 chars (compatible con mkdir que trunca a 11)
            std::string searchOld = (oldName.size() > 11) ? oldName.substr(0,11) : oldName;
            if (fb.b_content[j].b_inodo != -1 &&
                strncmp(fb.b_content[j].b_name, searchOld.c_str(), 11) == 0) {
                memset(fb.b_content[j].b_name, 0, 12);
                strncpy(fb.b_content[j].b_name, newName.c_str(), 11);
                ExtUtils::writeFolderBlock(file, sb, parentDir.i_block[i], fb);
                done = true;
            }
        }
    }

    if (!done) { file.close(); return "Error: no se pudo renombrar"; }

    inode.i_mtime = time(nullptr);
    ExtUtils::writeInode(file, sb, targetInode, inode);
    ExtUtils::journalAdd(file, sb, part.start, "rename", path, oldName + "->" + newName);
    file.close();
    return "rename: '" + path + "' renombrado a '" + newName + "'";
}

} // namespace CommandRename


// COPY — copia respetando permisos de lectura
// Soporta copiar a una carpeta existente (conserva nombre)
// o a una ruta nueva (cambia nombre)

namespace CommandCopy {

inline int copyRecursive(std::fstream& f, Superblock& sb, int partStart,
                          int srcInodeNum, int destDirInode,
                          const std::string& name, int uid, int gid) {
    Inode src = ExtUtils::readInode(f, sb, srcInodeNum);
    if (!ExtUtils::checkPermission(src, uid, gid, 'r')) return -1;

    int newInodeNum = ExtUtils::allocateBitmap(f, sb.s_bm_inode_start, sb.s_inodes_count);
    if (newInodeNum == -1) return -1;
    sb.s_free_inodes_count--;

    Inode newInode;
    newInode.i_uid = uid; newInode.i_gid = gid;
    newInode.i_size = 0;
    newInode.i_atime = newInode.i_ctime = newInode.i_mtime = time(nullptr);
    newInode.i_type = src.i_type;
    memcpy(newInode.i_perm, src.i_perm, 3);

    if (src.i_type == '0') {
        // Archivo: copiar contenido bloque a bloque
        std::string content = ExtUtils::readFileContent(f, sb, src);
        newInode.i_size = static_cast<int>(content.size());
        int bs = sb.s_block_size;
        int blocks = static_cast<int>(content.size() + bs - 1) / bs;
        for (int i = 0; i < blocks && i < 12; i++) {
            int bn = ExtUtils::allocateBitmap(f, sb.s_bm_block_start, sb.s_blocks_count);
            if (bn == -1) return -1;
            sb.s_free_blocks_count--;
            newInode.i_block[i] = bn;
            FileBlock fb;
            int off = i * bs;
            int tc  = std::min(bs, static_cast<int>(content.size()) - off);
            if (tc > 0) memcpy(fb.b_content, content.c_str() + off, tc);
            ExtUtils::writeFileBlock(f, sb, bn, fb);
        }
    } else {
        // Carpeta: crear bloque con . y .. y copiar hijos recursivamente
        int bn = ExtUtils::allocateBitmap(f, sb.s_bm_block_start, sb.s_blocks_count);
        if (bn == -1) return -1;
        sb.s_free_blocks_count--;
        newInode.i_block[0] = bn;

        FolderBlock nfb;
        strncpy(nfb.b_content[0].b_name, ".",  12); nfb.b_content[0].b_inodo = newInodeNum;
        strncpy(nfb.b_content[1].b_name, "..", 12); nfb.b_content[1].b_inodo = destDirInode;
        ExtUtils::writeFolderBlock(f, sb, bn, nfb);

        for (int i = 0; i < 12; i++) {
            if (src.i_block[i] == -1) break;
            FolderBlock sfb = ExtUtils::readFolderBlock(f, sb, src.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (sfb.b_content[j].b_inodo == -1) continue;
                std::string cname(sfb.b_content[j].b_name);
                if (cname == "." || cname == "..") continue;
                int nc = copyRecursive(f, sb, partStart,
                                       sfb.b_content[j].b_inodo,
                                       newInodeNum, cname, uid, gid);
                if (nc != -1)
                    ExtUtils::addEntryToDir(f, sb, partStart, newInodeNum, cname, nc);
            }
        }
    }

    ExtUtils::writeInode(f, sb, newInodeNum, newInode);
    ExtUtils::writeSuperblock(f, partStart, sb);
    return newInodeNum;
}

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string srcPath  = NewCmdUtils::getParam(commandLine, "-path");
    std::string destPath = NewCmdUtils::getParam(commandLine, "-destino");
    if (srcPath.empty())  return "Error: copy requiere -path";
    if (destPath.empty()) return "Error: copy requiere -destino";

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);
    int gid = NewCmdUtils::sessionGID(file, sb);

    // Obtener nombre del origen
    std::string srcParent, srcName;
    ExtUtils::splitPath(srcPath, srcParent, srcName);

    int srcInode = ExtUtils::traversePath(file, sb, srcPath);
    if (srcInode == -1) { file.close(); return "Error: origen '" + srcPath + "' no existe"; }

    // Determinar destino: puede ser carpeta existente o ruta nueva
    int destDirInode;
    std::string copyName;

    int destExisting = ExtUtils::traversePath(file, sb, destPath);
    if (destExisting != -1) {
        // El destino existe
        Inode destIn = ExtUtils::readInode(file, sb, destExisting);
        if (destIn.i_type == '1') {
            // Es una carpeta → copiar dentro con el mismo nombre
            destDirInode = destExisting;
            copyName     = srcName;
        } else {
            // Es un archivo existente → error (no sobrescribir)
            file.close();
            return "Error: el destino '" + destPath + "' ya existe";
        }
    } else {
        // El destino no existe → interpretar como ruta nueva (padre/nuevoNombre)
        std::string destParentPath, destNewName;
        ExtUtils::splitPath(destPath, destParentPath, destNewName);
        int destParentInode = (destParentPath == "/" || destParentPath.empty())
                              ? 0 : ExtUtils::traversePath(file, sb, destParentPath);
        if (destParentInode == -1) {
            file.close();
            return "Error: directorio destino '" + destParentPath + "' no existe";
        }
        destDirInode = destParentInode;
        copyName     = destNewName;
    }

    // Verificar permiso de escritura en el directorio destino
    Inode destDir = ExtUtils::readInode(file, sb, destDirInode);
    if (!ExtUtils::checkPermission(destDir, uid, gid, 'w')) {
        file.close();
        return "Error: sin permiso de escritura en destino '" + destPath + "'";
    }

    // Verificar que el nombre a copiar no exista ya en el destino
    if (ExtUtils::findInDir(file, sb, destDirInode, copyName) != -1) {
        file.close();
        return "Error: ya existe '" + copyName + "' en el destino";
    }

    int newInode = copyRecursive(file, sb, part.start, srcInode,
                                  destDirInode, copyName, uid, gid);
    if (newInode == -1) {
        file.close();
        return "Error: sin permiso de lectura en origen o sin espacio disponible";
    }

    ExtUtils::addEntryToDir(file, sb, part.start, destDirInode, copyName, newInode);
    ExtUtils::journalAdd(file, sb, part.start, "copy", srcPath, destPath);
    file.close();
    return "copy: '" + srcPath + "' copiado a '" + destPath + "'";
}

} // namespace CommandCopy


// MOVE — mueve cambiando solo referencias (misma partición)

namespace CommandMove {

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string srcPath  = NewCmdUtils::getParam(commandLine, "-path");
    std::string destPath = NewCmdUtils::getParam(commandLine, "-destino");
    if (srcPath.empty())  return "Error: move requiere -path";
    if (destPath.empty()) return "Error: move requiere -destino";

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);
    int gid = NewCmdUtils::sessionGID(file, sb);

    std::string srcParent, srcName;
    ExtUtils::splitPath(srcPath, srcParent, srcName);
    if (srcName.empty()) { file.close(); return "Error: ruta de origen invalida"; }

    int srcParentInode = (srcParent == "/" || srcParent.empty())
                         ? 0 : ExtUtils::traversePath(file, sb, srcParent);
    if (srcParentInode == -1) { file.close(); return "Error: directorio padre del origen no existe"; }

    int srcInode = ExtUtils::findInDir(file, sb, srcParentInode, srcName);
    if (srcInode == -1) { file.close(); return "Error: origen '" + srcPath + "' no existe"; }

    Inode srcIn = ExtUtils::readInode(file, sb, srcInode);
    if (!ExtUtils::checkPermission(srcIn, uid, gid, 'w')) {
        file.close();
        return "Error: sin permiso de escritura en origen '" + srcPath + "'";
    }

    // Determinar directorio destino y nombre final
    int destDirInode;
    std::string moveName;

    int destExisting = ExtUtils::traversePath(file, sb, destPath);
    if (destExisting != -1) {
        Inode destIn = ExtUtils::readInode(file, sb, destExisting);
        if (destIn.i_type == '1') {
            // Es una carpeta → mover dentro con el mismo nombre
            destDirInode = destExisting;
            moveName     = srcName;
        } else {
            file.close();
            return "Error: el destino '" + destPath + "' ya existe como archivo";
        }
    } else {
        // No existe → interpretar como ruta nueva (padre/nuevoNombre)
        std::string destParentPath, destNewName;
        ExtUtils::splitPath(destPath, destParentPath, destNewName);
        int destParentInode = (destParentPath == "/" || destParentPath.empty())
                              ? 0 : ExtUtils::traversePath(file, sb, destParentPath);
        if (destParentInode == -1) {
            file.close();
            return "Error: directorio destino '" + destParentPath + "' no existe";
        }
        destDirInode = destParentInode;
        moveName     = destNewName;
    }

    // Verificar permiso de escritura en carpeta destino
    Inode destDir = ExtUtils::readInode(file, sb, destDirInode);
    if (!ExtUtils::checkPermission(destDir, uid, gid, 'w')) {
        file.close();
        return "Error: sin permiso de escritura en destino";
    }

    if (ExtUtils::findInDir(file, sb, destDirInode, moveName) != -1) {
        file.close();
        return "Error: ya existe '" + moveName + "' en el destino";
    }

    // Agregar al destino y quitar del origen
    if (!ExtUtils::addEntryToDir(file, sb, part.start, destDirInode, moveName, srcInode)) {
        file.close();
        return "Error: no se pudo agregar al destino";
    }
    ExtUtils::removeEntryFromDir(file, sb, srcParentInode, srcName);

    // Si es carpeta, actualizar ".."
    if (srcIn.i_type == '1' && srcIn.i_block[0] != -1) {
        FolderBlock fb = ExtUtils::readFolderBlock(file, sb, srcIn.i_block[0]);
        for (int j = 0; j < 4; j++) {
            if (strncmp(fb.b_content[j].b_name, "..", 2) == 0) {
                fb.b_content[j].b_inodo = destDirInode;
                break;
            }
        }
        ExtUtils::writeFolderBlock(file, sb, srcIn.i_block[0], fb);
    }

    ExtUtils::journalAdd(file, sb, part.start, "move", srcPath, destPath);
    file.close();
    return "move: '" + srcPath + "' movido a '" + destPath + "'";
}

} // namespace CommandMove


// FIND — busca por nombre con wildcards ? y *

namespace CommandFind {

inline bool matchWildcard(const std::string& pattern, const std::string& text) {
    int m = static_cast<int>(pattern.size());
    int n = static_cast<int>(text.size());
    std::vector<std::vector<bool>> dp(m+1, std::vector<bool>(n+1, false));
    dp[0][0] = true;
    for (int i = 1; i <= m; i++)
        if (pattern[i-1] == '*') dp[i][0] = dp[i-1][0];
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++) {
            if (pattern[i-1] == '*')
                dp[i][j] = dp[i][j-1] || dp[i-1][j];
            else if (pattern[i-1] == '?' || pattern[i-1] == text[j-1])
                dp[i][j] = dp[i-1][j-1];
        }
    return dp[m][n];
}

inline void findRecursive(std::fstream& f, const Superblock& sb,
                           int dirInodeNum, const std::string& curPath,
                           const std::string& pattern, int uid, int gid,
                           std::vector<std::string>& results, int depth = 0) {
    if (depth > 50) return;
    Inode dir = ExtUtils::readInode(f, sb, dirInodeNum);
    if (!ExtUtils::checkPermission(dir, uid, gid, 'r')) return;
    for (int i = 0; i < 12; i++) {
        if (dir.i_block[i] == -1) break;
        FolderBlock fb = ExtUtils::readFolderBlock(f, sb, dir.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            std::string name(fb.b_content[j].b_name);
            if (name == "." || name == "..") continue;

            std::string fullPath = curPath;
            if (fullPath.empty() || fullPath.back() != '/') fullPath += "/";
            fullPath += name;

            Inode child = ExtUtils::readInode(f, sb, fb.b_content[j].b_inodo);
            if (!ExtUtils::checkPermission(child, uid, gid, 'r')) continue;

            if (matchWildcard(pattern, name)) results.push_back(fullPath);

            if (child.i_type == '1')
                findRecursive(f, sb, fb.b_content[j].b_inodo,
                              fullPath, pattern, uid, gid, results, depth+1);
        }
    }
}

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string searchPath = NewCmdUtils::getParam(commandLine, "-path");
    std::string pattern    = NewCmdUtils::getParam(commandLine, "-name");
    if (searchPath.empty()) return "Error: find requiere -path";
    if (pattern.empty())    return "Error: find requiere -name";

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);
    int gid = NewCmdUtils::sessionGID(file, sb);

    int startInode = (searchPath == "/") ? 0 : ExtUtils::traversePath(file, sb, searchPath);
    if (startInode == -1) {
        file.close();
        return "Error: ruta '" + searchPath + "' no existe";
    }

    Inode startDir = ExtUtils::readInode(file, sb, startInode);
    if (startDir.i_type != '1') {
        file.close();
        return "Error: '" + searchPath + "' no es un directorio";
    }

    std::vector<std::string> results;
    std::string basePath = (searchPath == "/") ? "" : searchPath;
    findRecursive(file, sb, startInode, basePath, pattern, uid, gid, results);
    file.close();

    std::ostringstream out;
    out << "\n=== FIND ===\n";
    out << "Patron: '" << pattern << "'  en: '" << searchPath << "'\n";
    out << "Resultados: " << results.size() << "\n";

    if (results.empty()) {
        out << "  No se encontraron coincidencias.\n";
    } else {
        out << searchPath << "\n";
        for (const auto& p : results) {
            // Calcular nivel de indentación
            size_t slashes = std::count(p.begin(), p.end(), '/');
            out << std::string(slashes * 2, ' ') << "|_ ";
            size_t ls = p.rfind('/');
            out << (ls == std::string::npos ? p : p.substr(ls+1)) << "\n";
        }
    }
    return out.str();
}

} // namespace CommandFind


// CHOWN — cambia propietario. Parámetro: -usuario=nombre
// Root puede cambiar cualquier archivo; otros solo los suyos.
// Flag -r para carpetas (recursivo).

namespace CommandChown {

inline void chownRecursive(std::fstream& f, const Superblock& sb,
                             int inodeNum, int newUid, int currentUid) {
    Inode inode = ExtUtils::readInode(f, sb, inodeNum);
    if (currentUid != 1 && inode.i_uid != currentUid) return;

    inode.i_uid   = newUid;
    inode.i_mtime = time(nullptr);
    ExtUtils::writeInode(f, sb, inodeNum, inode);

    if (inode.i_type == '1') {
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) break;
            FolderBlock fb = ExtUtils::readFolderBlock(f, sb, inode.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string n(fb.b_content[j].b_name);
                if (n == "." || n == "..") continue;
                chownRecursive(f, sb, fb.b_content[j].b_inodo, newUid, currentUid);
            }
        }
    }
}

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string path    = NewCmdUtils::getParam(commandLine, "-path");
    // Aceptar tanto -usuario= como -user= (compatibilidad con ambos scripts)
    std::string usuario = NewCmdUtils::getParam(commandLine, "-usuario");
    if (usuario.empty())
        usuario = NewCmdUtils::getParam(commandLine, "-user");

    bool recursive = NewCmdUtils::hasFlag(commandLine, "-r");

    if (path.empty())    return "Error: chown requiere -path";
    if (usuario.empty()) return "Error: chown requiere -usuario";

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);

    // Obtener UID del nuevo propietario
    int newUid = ExtUtils::getUserUID(file, sb, usuario);
    if (newUid == -1) {
        file.close();
        return "Error: usuario '" + usuario + "' no existe";
    }

    int targetInode = ExtUtils::traversePath(file, sb, path);
    if (targetInode == -1) {
        file.close();
        return "Error: '" + path + "' no existe";
    }

    Inode inode = ExtUtils::readInode(file, sb, targetInode);
    // Solo root o el propietario pueden hacer chown
    if (uid != 1 && inode.i_uid != uid) {
        file.close();
        return "Error: solo puede cambiar propietario de sus propios archivos/carpetas";
    }

    if (recursive && inode.i_type == '1') {
        chownRecursive(file, sb, targetInode, newUid, uid);
    } else {
        inode.i_uid   = newUid;
        inode.i_mtime = time(nullptr);
        ExtUtils::writeInode(file, sb, targetInode, inode);
    }

    ExtUtils::journalAdd(file, sb, part.start, "chown", path,
                         usuario + (recursive ? " -r" : ""));
    file.close();

    std::string result = "chown: propietario de '" + path + "' cambiado a '" + usuario + "'";
    if (recursive) result += " (recursivo)";
    return result;
}

} // namespace CommandChown


// CHMOD — cambia permisos. Solo el propietario o root.
// -ugo=XYZ (tres dígitos 0-7). Flag -r para recursivo.

namespace CommandChmod {

inline void chmodRecursive(std::fstream& f, const Superblock& sb,
                             int inodeNum, const char perm[3], int currentUid) {
    Inode inode = ExtUtils::readInode(f, sb, inodeNum);
    if (currentUid == 1 || inode.i_uid == currentUid) {
        memcpy(inode.i_perm, perm, 3);
        inode.i_mtime = time(nullptr);
        ExtUtils::writeInode(f, sb, inodeNum, inode);
    }
    if (inode.i_type == '1') {
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) break;
            FolderBlock fb = ExtUtils::readFolderBlock(f, sb, inode.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string n(fb.b_content[j].b_name);
                if (n == "." || n == "..") continue;
                chmodRecursive(f, sb, fb.b_content[j].b_inodo, perm, currentUid);
            }
        }
    }
}

inline std::string executeFromLine(const std::string& commandLine) {
    if (!currentSession.active) return "Error: debe iniciar sesion primero";

    std::string path = NewCmdUtils::getParam(commandLine, "-path");
    std::string ugo  = NewCmdUtils::getParam(commandLine, "-ugo");
    bool recursive   = NewCmdUtils::hasFlag(commandLine, "-r");

    if (path.empty()) return "Error: chmod requiere -path";
    if (ugo.empty())  return "Error: chmod requiere -ugo";

    // Validar tres dígitos 0-7
    if (ugo.size() != 3 ||
        ugo[0] < '0' || ugo[0] > '7' ||
        ugo[1] < '0' || ugo[1] > '7' ||
        ugo[2] < '0' || ugo[2] > '7') {
        return "Error: -ugo debe ser tres digitos entre 0 y 7 (ej: 764)";
    }

    CommandMount::MountedPartition part;
    std::fstream file;
    Superblock sb;
    if (!NewCmdUtils::openSession(file, sb, part))
        return "Error: no se pudo abrir la particion";

    int uid = NewCmdUtils::sessionUID(file, sb);

    int targetInode = ExtUtils::traversePath(file, sb, path);
    if (targetInode == -1) {
        file.close();
        return "Error: '" + path + "' no existe";
    }

    Inode inode = ExtUtils::readInode(file, sb, targetInode);
    if (uid != 1 && inode.i_uid != uid) {
        file.close();
        return "Error: '" + path + "' no pertenece al usuario actual";
    }

    char perm[3] = { ugo[0], ugo[1], ugo[2] };

    if (recursive && inode.i_type == '1') {
        chmodRecursive(file, sb, targetInode, perm, uid);
    } else {
        memcpy(inode.i_perm, perm, 3);
        inode.i_mtime = time(nullptr);
        ExtUtils::writeInode(file, sb, targetInode, inode);
    }

    ExtUtils::journalAdd(file, sb, part.start, "chmod", path,
                         ugo + (recursive ? " -r" : ""));
    file.close();

    std::string result = "chmod: permisos de '" + path + "' cambiados a " + ugo;
    if (recursive) result += " (recursivo)";
    return result;
}

} // namespace CommandChmod


// LOSS — Simula pérdida del sistema de archivos EXT3
// Limpia: bitmap inodos, bitmap bloques, área inodos, área bloques
// El journal queda intacto para permitir recovery

namespace CommandLoss {

inline std::string executeFromLine(const std::string& commandLine) {
    std::string id = NewCmdUtils::getParam(commandLine, "-id");
    if (id.empty()) return "Error: loss requiere -id";

    CommandMount::MountedPartition part;
    if (!CommandMount::getMountedPartition(id, part))
        return "Error: particion '" + id + "' no esta montada";

    std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open())
        return "Error: no se pudo abrir el disco '" + part.path + "'";

    Superblock sb = ExtUtils::readSuperblock(file, part.start);

    if (sb.s_filesystem_type != 3) {
        file.close();
        return "Error: loss solo aplica a particiones EXT3 (esta particion es EXT2)";
    }

    // Limpiar bitmap de inodos
    {
        file.seekp(sb.s_bm_inode_start, std::ios::beg);
        for (int i = 0; i < sb.s_inodes_count; i++) {
            char z = '\0'; file.write(&z, 1);
        }
    }
    // Limpiar bitmap de bloques
    {
        file.seekp(sb.s_bm_block_start, std::ios::beg);
        for (int i = 0; i < sb.s_blocks_count; i++) {
            char z = '\0'; file.write(&z, 1);
        }
    }
    // Limpiar área de inodos
    {
        int sz = sb.s_inodes_count * static_cast<int>(sizeof(Inode));
        file.seekp(sb.s_inode_start, std::ios::beg);
        for (int i = 0; i < sz; i++) { char z = '\0'; file.write(&z, 1); }
    }
    // Limpiar área de bloques
    {
        int sz = sb.s_blocks_count * sb.s_block_size;
        file.seekp(sb.s_block_start, std::ios::beg);
        for (int i = 0; i < sz; i++) { char z = '\0'; file.write(&z, 1); }
    }

    file.close();

    std::ostringstream out;
    out << "\n=== LOSS (Simulate System Loss) ===\n";
    out << "Particion: " << id << "\n";
    out << "Se limpiaron:\n";
    out << "  - Bitmap de inodos\n";
    out << "  - Bitmap de bloques\n";
    out << "  - Area de inodos\n";
    out << "  - Area de bloques\n";
    out << "Sistema de archivos EXT3 comprometido.\n";
    out << "El journal sigue disponible. Use 'journaling -id=" << id << "' para ver las transacciones.\n";
    return out.str();
}

} // namespace CommandLoss

#endif // NEW_COMMANDS_H