#ifndef REMOVE_H
#define REMOVE_H

/**
 * remove.h
 * Elimina un archivo o carpeta (y todo su contenido).
 */

#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include "../models/structs.h"
#include "../models/mounted_partitions.h"
#include "../session/session.h"
#include "ext_utils.h"
#include "journaling.h"

namespace CommandRemove {

    inline std::string parseParam(const std::string& line, const std::string& param) {
        std::string ll(line), pp(param);
        std::transform(ll.begin(), ll.end(), ll.begin(), ::tolower);
        std::transform(pp.begin(), pp.end(), pp.begin(), ::tolower);
        size_t pos = ll.find(pp + "=");
        if (pos == std::string::npos) return "";
        size_t start = pos + param.size() + 1;
        if (start >= line.size()) return "";
        if (line[start] == '"' || line[start] == '\'') {
            char q = line[start];
            size_t end = line.find(q, start + 1);
            return line.substr(start + 1, (end == std::string::npos ? line.size() : end) - start - 1);
        }
        size_t end = line.find(' ', start);
        return line.substr(start, (end == std::string::npos ? line.size() : end) - start);
    }

    // Eliminar recursivamente un inodo (árbol completo).
    // Retorna true si se pudo eliminar completamente.
    inline bool removeRecursive(std::fstream& file, Superblock& sb, int partStart,
                                 int inodeNum, int uid, int gid) {
        Inode inode = ExtUtils::readInode(file, sb, inodeNum);

        // Verificar permiso de escritura
        if (!ExtUtils::checkPermission(inode, uid, gid, 'w')) return false;

        if (inode.i_type == '1') {
            // Es carpeta: intentar eliminar cada hijo
            for (int i = 0; i < 12; i++) {
                if (inode.i_block[i] == -1) break;
                FolderBlock fb = ExtUtils::readFolderBlock(file, sb, inode.i_block[i]);
                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo == -1) continue;
                    std::string name(fb.b_content[j].b_name);
                    if (name == "." || name == "..") continue;
                    int childInode = fb.b_content[j].b_inodo;
                    if (!removeRecursive(file, sb, partStart, childInode, uid, gid))
                        return false; // No se puede eliminar hijo → no eliminar padre
                    // Marcar entry como vacío
                    memset(fb.b_content[j].b_name, 0, 12);
                    fb.b_content[j].b_inodo = -1;
                    ExtUtils::writeFolderBlock(file, sb, inode.i_block[i], fb);
                }
            }
        }

        // Liberar el inodo y sus bloques
        ExtUtils::freeInode(file, sb, partStart, inodeNum);
        return true;
    }

    inline std::string executeFromLine(const std::string& commandLine) {
        if (!CommandSession::isLoggedIn())
            return "Error: debe iniciar sesión primero";

        std::string path = parseParam(commandLine, "-path");
        if (path.empty()) return "Error: remove requiere -path";

        const auto& sess = CommandSession::getSession();
        MountedPartitions::MountedPartition part;
        std::fstream file;
        Superblock sb;
        if (!ExtUtils::openPartition(sess.mountId, file, sb, part))
            return "Error: no se pudo abrir la partición";

        // Navegar a la ruta
        std::string parentPath, name;
        ExtUtils::splitPath(path, parentPath, name);
        if (name.empty()) { file.close(); return "Error: ruta inválida"; }

        int parentInode = (parentPath == "/" || parentPath.empty())
                          ? 0 : ExtUtils::traversePath(file, sb, parentPath);
        if (parentInode == -1) { file.close(); return "Error: directorio padre no existe"; }

        int targetInode = ExtUtils::findInDir(file, sb, parentInode, name);
        if (targetInode == -1) { file.close(); return "Error: '" + path + "' no existe"; }

        // Eliminar recursivamente
        if (!removeRecursive(file, sb, part.start, targetInode, sess.uid, sess.gid)) {
            file.close();
            return "Error: no tiene permiso de escritura para eliminar '" + path + "'";
        }

        // Quitar del directorio padre
        ExtUtils::removeEntryFromDir(file, sb, parentInode, name);

        // Journal
        ExtUtils::journalAdd(file, sb, part.start, sess.mountId, "remove", path, "-");

        file.close();
        return "remove: '" + path + "' eliminado exitosamente";
    }

} // namespace CommandRemove

#endif // REMOVE_H