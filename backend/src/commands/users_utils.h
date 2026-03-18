#ifndef USERS_UTILS_H
#define USERS_UTILS_H


   // Helper compartido para leer y escribir users.txt en el disco.
   // Soporta contenido que ocupa múltiples bloques (directos + indirectos).
   // Usado por: mkgrp, rmgrp, mkusr, rmusr, chgrp, login.


#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include "../models/structs.h"

namespace UsersUtils {


  //  Lee el contenido completo de users.txt (inodo 1) desde el disco.
  //  Soporta bloques directos [0..11] e indirecto simple [12].

inline std::string readUsersFile(std::fstream& file, Superblock& sb) {

    // Inodo 1 = users.txt (siempre)
    Inode inode;
    file.seekg(sb.s_inode_start + 1 * sizeof(Inode));
    file.read((char*)&inode, sizeof(Inode));

    std::string content;
    int remaining = inode.i_size;

    // Bloques directos [0..11]
    for (int b = 0; b < 12 && remaining > 0; b++) {
        if (inode.i_block[b] == -1) continue;

        FileBlock fb;
        file.seekg(sb.s_block_start + inode.i_block[b] * sizeof(FileBlock));
        file.read((char*)&fb, sizeof(FileBlock));

        int chunk = std::min(64, remaining);
        content.append(fb.b_content, chunk);
        remaining -= chunk;
    }

    // Indirecto simple [12]
    if (inode.i_block[12] != -1 && remaining > 0) {
        PointerBlock pb;
        file.seekg(sb.s_block_start + inode.i_block[12] * sizeof(PointerBlock));
        file.read((char*)&pb, sizeof(PointerBlock));

        for (int i = 0; i < 16 && remaining > 0; i++) {
            if (pb.b_pointers[i] == -1) continue;

            FileBlock fb;
            file.seekg(sb.s_block_start + pb.b_pointers[i] * sizeof(FileBlock));
            file.read((char*)&fb, sizeof(FileBlock));

            int chunk = std::min(64, remaining);
            content.append(fb.b_content, chunk);
            remaining -= chunk;
        }
    }

    return content;
}


   // Busca el siguiente bloque libre en el bitmap.

inline int findFreeBlock(std::fstream& file, Superblock& sb) {
    file.seekg(sb.s_bm_block_start);
    for (int i = 0; i < sb.s_blocks_count; i++) {
        char bit; file.read(&bit, 1);
        if (bit == '0') return i;
    }
    return -1;
}


   // Escribe el contenido de users.txt de vuelta al disco.
   // Reutiliza bloques ya asignados y asigna nuevos si el contenido creció.
   // Actualiza el inodo (i_size) y el superbloque (free counts).

inline bool writeUsersFile(std::fstream& file, Superblock& sb, int partStart,
                            const std::string& content) {

    // Leer inodo 1
    Inode inode;
    file.seekg(sb.s_inode_start + 1 * sizeof(Inode));
    file.read((char*)&inode, sizeof(Inode));

    int totalBytes = (int)content.size();
    int written = 0;
    int blockNum = 0;  // índice lógico de bloque dentro del archivo

    while (written < totalBytes) {

        int chunkSize = std::min(64, totalBytes - written);

        //  Bloque directo [0..11] 
        if (blockNum < 12) {

            if (inode.i_block[blockNum] == -1) {
                // Asignar nuevo bloque
                int newBlock = findFreeBlock(file, sb);
                if (newBlock == -1) return false;

                char used = '1';
                file.seekp(sb.s_bm_block_start + newBlock);
                file.write(&used, 1);
                sb.s_free_blocks_count--;

                inode.i_block[blockNum] = newBlock;
            }

            FileBlock fb;
            memset(fb.b_content, 0, 64);
            memcpy(fb.b_content, content.c_str() + written, chunkSize);

            file.seekp(sb.s_block_start + inode.i_block[blockNum] * sizeof(FileBlock));
            file.write((char*)&fb, sizeof(FileBlock));

        }
        //  Indirecto simple [12] 
        else if (blockNum < 12 + 16) {

            // Crear bloque de punteros si no existe
            if (inode.i_block[12] == -1) {
                int ptrBlock = findFreeBlock(file, sb);
                if (ptrBlock == -1) return false;

                PointerBlock pb;  // ya inicializado con -1
                file.seekp(sb.s_block_start + ptrBlock * sizeof(PointerBlock));
                file.write((char*)&pb, sizeof(PointerBlock));

                char used = '1';
                file.seekp(sb.s_bm_block_start + ptrBlock);
                file.write(&used, 1);
                sb.s_free_blocks_count--;

                inode.i_block[12] = ptrBlock;
            }

            // Leer bloque de punteros
            PointerBlock pb;
            file.seekg(sb.s_block_start + inode.i_block[12] * sizeof(PointerBlock));
            file.read((char*)&pb, sizeof(PointerBlock));

            int ptrIdx = blockNum - 12;

            if (pb.b_pointers[ptrIdx] == -1) {
                int newBlock = findFreeBlock(file, sb);
                if (newBlock == -1) return false;

                char used = '1';
                file.seekp(sb.s_bm_block_start + newBlock);
                file.write(&used, 1);
                sb.s_free_blocks_count--;

                pb.b_pointers[ptrIdx] = newBlock;

                // Guardar bloque de punteros actualizado
                file.seekp(sb.s_block_start + inode.i_block[12] * sizeof(PointerBlock));
                file.write((char*)&pb, sizeof(PointerBlock));
            }

            FileBlock fb;
            memset(fb.b_content, 0, 64);
            memcpy(fb.b_content, content.c_str() + written, chunkSize);

            file.seekp(sb.s_block_start + pb.b_pointers[ptrIdx] * sizeof(FileBlock));
            file.write((char*)&fb, sizeof(FileBlock));

        } else {
            // Archivo demasiado grande (>1792 bytes) — inusual para users.txt
            return false;
        }

        written += chunkSize;
        blockNum++;
    }

    // Actualizar tamaño en el inodo
    inode.i_size = totalBytes;

    file.seekp(sb.s_inode_start + 1 * sizeof(Inode));
    file.write((char*)&inode, sizeof(Inode));

    // Actualizar superbloque
    file.seekp(partStart);
    file.write((char*)&sb, sizeof(Superblock));

    return true;
}

} // namespace UsersUtils

#endif