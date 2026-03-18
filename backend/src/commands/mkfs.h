#ifndef MKFS_H
#define MKFS_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <ctime>

#include "../models/structs.h"
#include "mount.h"

namespace CommandMkfs {

/*
    Convierte una cadena a minúsculas
*/
inline std::string toLowerCase(std::string str){
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

/*
    Comando MKFS
    Crea un sistema de archivos EXT2 en una partición montada
*/
inline std::string execute(const std::string& id, const std::string& type){

    if(id.empty()){
        return "Error: mkfs requiere el parametro -id";
    }

    std::string formatType = toLowerCase(type);

    if(formatType.empty()){
        formatType = "full";
    }

    if(formatType != "full" && formatType != "fast"){
        return "Error: -type solo puede ser full o fast";
    }

    /*
        Buscar la partición montada
    */
    CommandMount::MountedPartition partition;

    if(!CommandMount::getMountedPartition(id, partition)){
        return "Error: la particion con ID '" + id + "' no esta montada";
    }

    /*
        Abrir disco
    */
    std::fstream file(partition.path, std::ios::in | std::ios::out | std::ios::binary);

    if(!file.is_open()){
        return "Error: no se pudo abrir el disco";
    }

    int partitionSize = partition.size;
    int start = partition.start;

    /*
        FORMATEO FULL
        Limpia completamente la partición
    */
    if(formatType == "full"){

        file.seekp(start);

        char zero = '\0';

        for(int i = 0; i < partitionSize; i++){
            file.write(&zero,1);
        }
    }

    /*
        Calcular número de estructuras
    */

    int numerator = partitionSize - sizeof(Superblock);

    int denominator =
        4 +
        sizeof(Inode) +
        (3 * sizeof(FileBlock));

    int n = numerator / denominator;

    if(n <= 0){
        file.close();
        return "Error: particion demasiado pequena";
    }

    /*
        Crear Superbloque
    */

    Superblock sb;

    sb.s_filesystem_type = 2;
    sb.s_inodes_count = n;
    sb.s_blocks_count = 3 * n;

    sb.s_free_blocks_count = (3 * n) - 2;
    sb.s_free_inodes_count = n - 2;

    sb.s_mtime = time(nullptr);
    sb.s_umtime = time(nullptr);

    sb.s_mnt_count = 1;

    sb.s_magic = 0xEF53;

    sb.s_inode_size = sizeof(Inode);
    sb.s_block_size = sizeof(FileBlock);

    sb.s_first_ino = 2;
    sb.s_first_blo = 2;

    sb.s_bm_inode_start = start + sizeof(Superblock);

    sb.s_bm_block_start = sb.s_bm_inode_start + n;

    sb.s_inode_start = sb.s_bm_block_start + (3 * n);

    sb.s_block_start = sb.s_inode_start + (n * sizeof(Inode));

    /*
        Escribir Superbloque
    */

    file.seekp(start);
    file.write((char*)&sb,sizeof(Superblock));

    /*
        Inicializar bitmap de inodos
    */

    file.seekp(sb.s_bm_inode_start);

    for(int i=0;i<n;i++){

        char bit='0';

        if(i==0 || i==1)
            bit='1';

        file.write(&bit,1);
    }

    /*
        Inicializar bitmap de bloques
    */

    file.seekp(sb.s_bm_block_start);

    for(int i=0;i<3*n;i++){

        char bit='0';

        if(i==0 || i==1)
            bit='1';

        file.write(&bit,1);
    }

    /*
        Crear Inodo RAIZ
    */

    Inode root;

    root.i_uid=1;
    root.i_gid=1;
    root.i_size=0;

    root.i_atime=time(nullptr);
    root.i_ctime=time(nullptr);
    root.i_mtime=time(nullptr);

    root.i_type='1';

    root.i_block[0]=0;

    file.seekp(sb.s_inode_start);
    file.write((char*)&root,sizeof(Inode));

    /*
        Crear Inodo users.txt
    */

    Inode users;

    users.i_uid=1;
    users.i_gid=1;

    users.i_size=27;

    users.i_atime=time(nullptr);
    users.i_ctime=time(nullptr);
    users.i_mtime=time(nullptr);

    users.i_type='0';

    users.i_block[0]=1;

    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write((char*)&users,sizeof(Inode));

    /*
        Crear Bloque carpeta raiz
    */

    FolderBlock rootBlock;

    strcpy(rootBlock.b_content[0].b_name,".");
    rootBlock.b_content[0].b_inodo=0;

    strcpy(rootBlock.b_content[1].b_name,"..");
    rootBlock.b_content[1].b_inodo=0;

    strcpy(rootBlock.b_content[2].b_name,"users.txt");
    rootBlock.b_content[2].b_inodo=1;

    rootBlock.b_content[3].b_inodo=-1;

    file.seekp(sb.s_block_start);
    file.write((char*)&rootBlock,sizeof(FolderBlock));

    /*
        Crear Bloque users.txt
    */

    FileBlock usersBlock;

    std::string usersContent =
        "1,G,root\n"
        "1,U,root,root,123\n";

    strncpy(usersBlock.b_content,usersContent.c_str(),64);

    file.seekp(sb.s_block_start + sizeof(FolderBlock));
    file.write((char*)&usersBlock,sizeof(FileBlock));

    file.close();

    /*
        Resultado
    */

    std::ostringstream result;

    result<<"\n=== MKFS ===\n";
    result<<"Sistema de archivos EXT2 creado correctamente\n";
    result<<"ID: "<<id<<"\n";
    result<<"Disco: "<<partition.path<<"\n";
    result<<"Particion: "<<partition.name<<"\n";
    result<<"Tipo formateo: "<<formatType<<"\n";
    result<<"Inodos: "<<n<<"\n";
    result<<"Bloques: "<<(3*n)<<"\n";
    result<<"Archivo users.txt creado\n";

    return result.str();
}


    // Parser del comando mkfs desde una linea completa

inline std::string executeFromLine(const std::string& commandLine){

    std::istringstream iss(commandLine);
    std::string token;

    std::string id = "";
    std::string type = "";

    iss >> token; // saltar "mkfs"

    while(iss >> token){

        std::string lower = toLowerCase(token);

        if(lower.find("-id=") == 0){
            id = token.substr(4);
        }
        else if(lower.find("-type=") == 0){
            type = token.substr(6);
        }
    }

    return execute(id,type);
}



}

#endif