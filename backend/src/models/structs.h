#ifndef STRUCTS_H
#define STRUCTS_H

#include <ctime>
#include <cstdlib>
#include <cstring>

#pragma pack(push, 1)

// PARTITION
struct Partition {
    char part_status;          // 0 = no montada, 1 = montada
    char part_type;            // P = primaria, E = extendida
    char part_fit;             // B, F, W
    int part_start;            // byte donde inicia
    int part_s;                // tamaño en bytes
    char part_name[16];        // nombre
    int part_correlative;      // correlativo al montar
    char part_id[4];           // ID generado en mount

    Partition() {
        part_status = '0';
        part_type = 'P';
        part_fit = 'F';
        part_start = -1;
        part_s = 0;
        part_correlative = -1;
        memset(part_name, 0, sizeof(part_name));
        memset(part_id, 0, sizeof(part_id));
    }
};

// MBR
struct MBR {
    int mbr_tamano;                 // tamaño total del disco en bytes
    time_t mbr_fecha_creacion;      // fecha creación
    int mbr_dsk_signature;          // número random único
    char dsk_fit;                   // B, F o W
    Partition mbr_partitions[4];    // máximo 4 particiones

    MBR() {
        mbr_tamano = 0;
        mbr_fecha_creacion = time(nullptr);
        mbr_dsk_signature = rand();
        dsk_fit = 'F';
    }
};

// EBR
struct EBR {
    char part_status;          // Estado de la partición lógica
    char part_fit;             // Ajuste de la partición
    int part_start;            // Byte donde inicia la partición lógica
    int part_size;             // Tamaño de la partición lógica
    int part_next;             // Byte donde inicia el siguiente EBR (-1 si no hay más)
    char part_name[16];        // Nombre de la partición

    EBR() {
        part_status = '0';
        part_fit = 'F';
        part_start = -1;
        part_size = 0;
        part_next = -1;
        memset(part_name, 0, sizeof(part_name));
    }
};

// ESTRUCTURAS DEL SISTEMA DE ARCHIVOS 


// SUPERBLOQUE
struct Superblock {

    int s_filesystem_type;     
    int s_inodes_count;        
    int s_blocks_count;        
    int s_free_blocks_count;   
    int s_free_inodes_count;   

    time_t s_mtime;            
    time_t s_umtime;           
    int s_mnt_count;           

    int s_magic;               

    int s_inode_size;          
    int s_block_size;          

    int s_first_ino;           
    int s_first_blo;           

    int s_bm_inode_start;      
    int s_bm_block_start;      

    int s_inode_start;         
    int s_block_start;         

    Superblock() {

        s_filesystem_type = 0;
        s_inodes_count = 0;
        s_blocks_count = 0;

        s_free_blocks_count = 0;
        s_free_inodes_count = 0;

        s_mtime = time(nullptr);
        s_umtime = time(nullptr);

        s_mnt_count = 0;

        s_magic = 0xEF53;

        s_inode_size = 0;
        s_block_size = 64;

        s_first_ino = 0;
        s_first_blo = 0;

        s_bm_inode_start = 0;
        s_bm_block_start = 0;

        s_inode_start = 0;
        s_block_start = 0;
    }
};


// INODO
struct Inode {

    int i_uid;
    int i_gid;
    int i_size;

    time_t i_atime;
    time_t i_ctime;
    time_t i_mtime;

    int i_block[15];

    char i_type;     // 0 archivo | 1 carpeta
    int i_perm;

    Inode() {

        i_uid = 0;
        i_gid = 0;
        i_size = 0;

        i_atime = time(nullptr);
        i_ctime = time(nullptr);
        i_mtime = time(nullptr);

        for (int i = 0; i < 15; i++) {
            i_block[i] = -1;
        }

        i_type = '0';
        i_perm = 664;
    }
};


// CONTENT
struct Content {

    char b_name[12];
    int b_inodo;

    Content() {
        memset(b_name, 0, sizeof(b_name));
        b_inodo = -1;
    }
};


// BLOQUE DE CARPETA
struct FolderBlock {

    Content b_content[4];

    FolderBlock() {
    }
};


// BLOQUE DE ARCHIVO
struct FileBlock {

    char b_content[64];

    FileBlock() {
        memset(b_content, 0, sizeof(b_content));
    }
};


// BLOQUE DE APUNTADORES
struct PointerBlock {

    int b_pointers[16];

    PointerBlock() {
        for (int i = 0; i < 16; i++) {
            b_pointers[i] = -1;
        }
    }
};


#pragma pack(pop)

#endif