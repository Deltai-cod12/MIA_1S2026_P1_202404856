#ifndef STRUCTS_H
#define STRUCTS_H

#include <ctime>
#include <cstdlib>
#include <cstring>

#pragma pack(push, 1)

// ============================================================
// PARTITION
// ============================================================
struct Partition {
    char part_status;       // '0' = inactiva, '1' = activa
    char part_type;         // 'P', 'E', 'L'
    char part_fit;          // 'B', 'F', 'W'
    int  part_start;        // byte donde inicia
    int  part_s;            // tamaño en bytes
    char part_name[16];     // nombre
    int  part_correlative;  // correlativo al montar
    char part_id[4];        // ID generado en mount

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

// ============================================================
// MBR
// ============================================================
struct MBR {
    int    mbr_tamano;
    time_t mbr_fecha_creacion;
    int    mbr_dsk_signature;
    char   dsk_fit;
    Partition mbr_partitions[4];

    MBR() {
        mbr_tamano         = 0;
        mbr_fecha_creacion = time(nullptr);
        mbr_dsk_signature  = rand();
        dsk_fit            = 'F';
    }
};

// ============================================================
// EBR
// ============================================================
struct EBR {
    char part_status;
    char part_fit;
    int  part_start;
    int  part_size;
    int  part_next;
    char part_name[16];

    EBR() {
        part_status = '0';
        part_fit    = 'F';
        part_start  = -1;
        part_size   = 0;
        part_next   = -1;
        memset(part_name, 0, sizeof(part_name));
    }
};

// ============================================================
// EXT3 — Journal structures
// ============================================================

// Information: contenido de una entrada del journal
struct Information {
    char  i_operation[10]; // operación: mkdir, mkfile, remove...
    char  i_path[32];      // ruta donde se realizó
    char  i_content[64];   // contenido del archivo o "-"
    float i_date;          // fecha como float (cast de time_t)

    Information() {
        memset(i_operation, 0, sizeof(i_operation));
        memset(i_path,      0, sizeof(i_path));
        memset(i_content,   0, sizeof(i_content));
        i_date = 0.0f;
    }
};

// Journal: una entrada completa
struct Journal {
    int         j_count;   // número correlativo de la entrada
    Information j_content; // información de la operación

    Journal() {
        j_count = 0;
    }
};

// Constante: máximo de entradas de journal por partición
#define JOURNAL_MAX 50

// ============================================================
// SUPERBLOQUE (EXT2 y EXT3)
// ============================================================
struct Superblock {
    int    s_filesystem_type;   // 2=EXT2, 3=EXT3
    int    s_inodes_count;
    int    s_blocks_count;
    int    s_free_blocks_count;
    int    s_free_inodes_count;
    time_t s_mtime;
    time_t s_umtime;
    int    s_mnt_count;
    int    s_magic;             // 0xEF53
    int    s_inode_size;
    int    s_block_size;
    int    s_first_ino;
    int    s_first_blo;
    int    s_bm_inode_start;
    int    s_bm_block_start;
    int    s_inode_start;
    int    s_block_start;
    // Campos exclusivos EXT3:
    int    s_journal_start;     // inicio del área de journaling
    int    s_journal_count;     // entradas usadas

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
        s_journal_start     = 0;
        s_journal_count     = 0;
    }
};

// ============================================================
// INODO
// ============================================================
struct Inode {
    int    i_uid;
    int    i_gid;
    int    i_size;
    time_t i_atime;
    time_t i_ctime;
    time_t i_mtime;
    int    i_block[15];
    char   i_type;    // '0'=archivo, '1'=carpeta
    char   i_perm[3]; // ej: "664"

    Inode() {
        i_uid   = 0;
        i_gid   = 0;
        i_size  = 0;
        i_atime = time(nullptr);
        i_ctime = time(nullptr);
        i_mtime = time(nullptr);
        for (int i = 0; i < 15; i++) i_block[i] = -1;
        i_type = '0';
        memcpy(i_perm, "664", 3);
    }
};

// ============================================================
// BLOQUES
// ============================================================
struct Content {
    char b_name[12];
    int  b_inodo;

    Content() {
        memset(b_name, 0, sizeof(b_name));
        b_inodo = -1;
    }
};

// Bloque carpeta: 4 entradas × 16 bytes = 64 bytes
struct FolderBlock {
    Content b_content[4];
    FolderBlock() {}
};

// Bloque archivo: 64 bytes
struct FileBlock {
    char b_content[64];
    FileBlock() { memset(b_content, 0, sizeof(b_content)); }
};

// Bloque de apuntadores: 16 × 4 bytes = 64 bytes
struct PointerBlock {
    int b_pointers[16];
    PointerBlock() { for (int i = 0; i < 16; i++) b_pointers[i] = -1; }
};

#pragma pack(pop)

#endif // STRUCTS_H