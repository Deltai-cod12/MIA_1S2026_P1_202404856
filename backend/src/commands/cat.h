#ifndef CAT_H
#define CAT_H

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include "../models/structs.h"
#include "../models/mounted_partitions.h"
#include "../session/session.h"

using namespace std;

namespace CommandCat {

    // OBTENER PARAMETROS -fileN
    vector<string> getFiles(const string& line){

        vector<string> files;

        for(int i=1;i<=20;i++){

            string key="-file"+to_string(i)+"=";
            size_t pos=line.find(key);

            if(pos==string::npos) continue;

            pos+=key.length();

            size_t end=line.find(" ",pos);

            if(end==string::npos)
                end=line.length();

            string path=line.substr(pos,end-pos);

            if(path.front()=='"' && path.back()=='"')
                path=path.substr(1,path.size()-2);

            files.push_back(path);
        }

        return files;
    }

    // DIVIDIR PATH
    vector<string> splitPath(string path){

        vector<string> parts;
        string temp="";

        for(char c:path){

            if(c=='/'){
                if(!temp.empty()){
                    parts.push_back(temp);
                    temp="";
                }
            }else{
                temp+=c;
            }
        }

        if(!temp.empty())
            parts.push_back(temp);

        return parts;
    }

    // BUSCAR INODO POR PATH
    int searchInode(FILE* disk, Superblock& sb, int inodeIndex, vector<string>& pathParts, int level){

        if(level==pathParts.size())
            return inodeIndex;

        Inode inode;

        fseek(disk, sb.s_inode_start + inodeIndex*sizeof(Inode), SEEK_SET);
        fread(&inode,sizeof(Inode),1,disk);

        for(int i=0;i<12;i++){

            if(inode.i_block[i]==-1)
                continue;

            FolderBlock folder;

            fseek(disk, sb.s_block_start + inode.i_block[i]*sizeof(FolderBlock), SEEK_SET);
            fread(&folder,sizeof(FolderBlock),1,disk);

            for(int j=0;j<4;j++){

                if(folder.b_content[j].b_inodo==-1)
                    continue;

                if(strncmp(folder.b_content[j].b_name, pathParts[level].c_str(), 11)==0){

                    return searchInode(
                        disk,
                        sb,
                        folder.b_content[j].b_inodo,
                        pathParts,
                        level+1
                    );
                }
            }
        }

        return -1;
    }

    // LEER BLOQUES DEL ARCHIVO
    string readFileContent(FILE* disk, Superblock& sb, int inodeIndex){

        Inode inode;

        fseek(disk, sb.s_inode_start + inodeIndex*sizeof(Inode), SEEK_SET);
        fread(&inode,sizeof(Inode),1,disk);

        string content="";

        // BLOQUES DIRECTOS
        for(int i=0;i<12;i++){

            if(inode.i_block[i]==-1)
                continue;

            FileBlock block;

            fseek(disk, sb.s_block_start + inode.i_block[i]*sizeof(FileBlock), SEEK_SET);
            fread(&block,sizeof(FileBlock),1,disk);

            content.append(block.b_content, strnlen(block.b_content,64));
        }

        // BLOQUE INDIRECTO SIMPLE
        if(inode.i_block[12] != -1){

            PointerBlock pblock;

            fseek(disk, sb.s_block_start + inode.i_block[12]*sizeof(PointerBlock), SEEK_SET);
            fread(&pblock,sizeof(PointerBlock),1,disk);

            for(int i=0;i<16;i++){

                if(pblock.b_pointers[i]==-1)
                    continue;

                FileBlock block;

                fseek(disk, sb.s_block_start + pblock.b_pointers[i]*sizeof(FileBlock), SEEK_SET);
                fread(&block,sizeof(FileBlock),1,disk);

                content.append(block.b_content, strnlen(block.b_content,64));
            }
        }

        return content;
    }

    // OBTENER PARTICION MONTADA — usar sesión activa
    MountedPartition getMounted(){
        if(!currentSession.active)
            throw runtime_error("Error: no hay sesion activa");

        for(auto& mp : mountedPartitions)
            if(mp.id == currentSession.id)
                return mp;

        throw runtime_error("Error: particion de la sesion no montada");
    }

    // EJECUTAR CAT
    string execute(const string& line){

        vector<string> files=getFiles(line);

        if(files.empty())
            return "Error: debe especificar al menos un parametro -fileN";

        MountedPartition mp;

        try{
            mp=getMounted();
        }catch(exception& e){
            return e.what();
        }

        FILE* disk=fopen(mp.path.c_str(),"rb");

        if(!disk)
            return "Error: no se pudo abrir el disco";

        Superblock sb;

        fseek(disk, mp.start, SEEK_SET);
        fread(&sb,sizeof(Superblock),1,disk);

        string result="";

        for(size_t i=0;i<files.size();i++){

            vector<string> parts=splitPath(files[i]);

            int inodeIndex = searchInode(
                disk,
                sb,
                0,
                parts,
                0
            );

            if(inodeIndex==-1){

                fclose(disk);
                return "Error: archivo no encontrado -> "+files[i];
            }

            string content = readFileContent(
                disk,
                sb,
                inodeIndex
            );

            result += content;

            if(i!=files.size()-1)
                result += "\n";
        }

        fclose(disk);

        return result;
    }

}

#endif