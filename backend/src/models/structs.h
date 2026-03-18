#ifndef STRUCTS_H
#define STRUCTS_H

#include <ctime>
#include <cstdlib>
#include <cstring>

#pragma pack(push, 1)

// PARTITION
struct Partition {
    char part_status;          // '0' = inactiva, '1' = activa
    char part_type;            // 'P' = primaria, 'E' = extendida, 'L' = lógica
    char part_fit;             // 'B', 'F', 'W'
    int  part_start;           // byte donde inicia
    int  part_s;               // tamaño en bytes
    char part_name[16];        // nombre
    int  part_correlative;     // correlativo al montar
    char part_id[4];           // ID generado en mount

    Partition() {
        part_status      = '0';
        part_type        = 'P';
        part_fit         = 'F';
        part_start       = -1;
        part_s           = 0;
        part_correlative = -1;
        memset(part_name, 0, sizeof(part_name));
        memset(part_id,   0, sizeof(part_id));
    }
};

// MBR
struct MBR {
    int    mbr_tamano;              // tamaño total del disco en bytes
    time_t mbr_fecha_creacion;      // fecha creación
    int    mbr_dsk_signature;       // número random único
    char   dsk_fit;                 // 'B', 'F' o 'W'
    Partition mbr_partitions[4];    // máximo 4 particiones primarias/extendidas

    MBR() {
        mbr_tamano         = 0;
        mbr_fecha_creacion = time(nullptr);
        mbr_dsk_signature  = rand();
        dsk_fit            = 'F';
    }
};

// EBR
struct EBR {
    char part_status;   // '0' = libre, '1' = ocupada
    char part_fit;      // 'B', 'F', 'W'
    int  part_start;    // byte donde inicia la partición lógica
    int  part_size;     // tamaño en bytes
    int  part_next;     // byte del siguiente EBR, -1 si no hay más
    char part_name[16]; // nombre

    EBR() {
        part_status = '0';
        part_fit    = 'F';
        part_start  = -1;
        part_size   = 0;
        part_next   = -1;
        memset(part_name, 0, sizeof(part_name));
    }
};

// ESTRUCTURAS DEL SISTEMA DE ARCHIVOS 

// SUPERBLOQUE
struct Superblock {
    int    s_filesystem_type;   // 2 = EXT2
    int    s_inodes_count;      // total de inodos
    int    s_blocks_count;      // total de bloques
    int    s_free_blocks_count; // bloques libres
    int    s_free_inodes_count; // inodos libres
    time_t s_mtime;             // último montaje
    time_t s_umtime;            // último desmontaje
    int    s_mnt_count;         // veces montado
    int    s_magic;             // 0xEF53
    int    s_inode_size;        // sizeof(Inode)
    int    s_block_size;        // sizeof(FileBlock) = 64
    int    s_first_ino;         // primer inodo libre
    int    s_first_blo;         // primer bloque libre
    int    s_bm_inode_start;    // inicio bitmap inodos
    int    s_bm_block_start;    // inicio bitmap bloques
    int    s_inode_start;       // inicio tabla de inodos
    int    s_block_start;       // inicio tabla de bloques

    Superblock() {
        s_filesystem_type   = 0;
        s_inodes_count      = 0;
        s_blocks_count      = 0;
        s_free_blocks_count = 0;
        s_free_inodes_count = 0;
        s_mtime             = time(nullptr);
        s_umtime            = time(nullptr);
        s_mnt_count         = 0;
        s_magic             = 0xEF53;
        s_inode_size        = 0;
        s_block_size        = 64;
        s_first_ino         = 0;
        s_first_blo         = 0;
        s_bm_inode_start    = 0;
        s_bm_block_start    = 0;
        s_inode_start       = 0;
        s_block_start       = 0;
    }
};

// INODO
struct Inode {
    int    i_uid;        // UID del propietario
    int    i_gid;        // GID del grupo
    int    i_size;       // tamaño en bytes
    time_t i_atime;      // último acceso
    time_t i_ctime;      // creación
    time_t i_mtime;      // última modificación
    int    i_block[15];  // [0..11] directos, [12] simple, [13] doble, [14] triple
    char   i_type;       // '0' = archivo, '1' = carpeta
    char   i_perm[3];    // permisos ej: "664"

    Inode() {
        i_uid   = 0;
        i_gid   = 0;
        i_size  = 0;
        i_atime = time(nullptr);
        i_ctime = time(nullptr);
        i_mtime = time(nullptr);
        for (int i = 0; i < 15; i++)
            i_block[i] = -1;
        i_type = '0';
        memcpy(i_perm, "664", 3);  //  correcto: copia 3 chars sin null terminator
    }
};

// CONTENT (entrada de directorio)
struct Content {
    char b_name[12];  // nombre del archivo/carpeta
    int  b_inodo;     // índice del inodo (-1 = vacío)

    Content() {
        memset(b_name, 0, sizeof(b_name));
        b_inodo = -1;
    }
};

// BLOQUE DE CARPETA — 4 entradas × 16 bytes = 64 bytes 
struct FolderBlock {
    Content b_content[4];
    FolderBlock() {}
};

// BLOQUE DE ARCHIVO — 64 bytes 
struct FileBlock {
    char b_content[64];
    FileBlock() {
        memset(b_content, 0, sizeof(b_content));
    }
};

// BLOQUE DE APUNTADORES — 16 × 4 bytes = 64 bytes 
struct PointerBlock {
    int b_pointers[16];
    PointerBlock() {
        for (int i = 0; i < 16; i++)
            b_pointers[i] = -1;
    }
};

#pragma pack(pop)

#endif