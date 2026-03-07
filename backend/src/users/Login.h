#ifndef LOGIN_H
#define LOGIN_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "../models/structs.h"
#include "../commands/mount.h"
#include "../session/session.h"

namespace CommandLogin {

inline std::vector<std::string> split(const std::string &str,char delimiter){

    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;

    while(std::getline(ss,item,delimiter)){
        tokens.push_back(item);
    }

    return tokens;
}

inline std::string execute(const std::string& user,const std::string& pass,const std::string& id){

    if(currentSession.active){
        return "Error: ya existe una sesion activa";
    }

    CommandMount::MountedPartition partition;

    if(!CommandMount::getMountedPartition(id,partition)){
        return "Error: particion no montada";
    }

    std::fstream file(partition.path,std::ios::in | std::ios::binary);

    if(!file.is_open()){
        return "Error: no se pudo abrir el disco";
    }

    Superblock sb;

    file.seekg(partition.start);
    file.read((char*)&sb,sizeof(Superblock));

    Inode inode;

    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read((char*)&inode,sizeof(Inode));

    FileBlock block;

    file.seekg(sb.s_block_start + sizeof(FolderBlock));
    file.read((char*)&block,sizeof(FileBlock));

    std::string content(block.b_content);

    std::stringstream ss(content);

    std::string line;

    while(std::getline(ss,line)){

        std::vector<std::string> data = split(line,',');

        if(data.size()==5){

            if(data[1]=="U"){

                if(data[3]==user && data[4]==pass){

                    currentSession.active = true;
                    currentSession.user = user;
                    currentSession.group = data[2];
                    currentSession.id = id;

                    file.close();

                    return "Login exitoso";
                }

            }

        }

    }

    file.close();

    return "Error: usuario o contraseña incorrectos";
}

/*
    Parsear linea completa del comando login
*/
inline std::string executeFromLine(const std::string& commandLine){

    std::istringstream iss(commandLine);
    std::string token;

    std::string user;
    std::string pass;
    std::string id;

    while(iss >> token){

        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if(lower.find("-user=") == 0){
            user = token.substr(6);
        }
        else if(lower.find("-pass=") == 0){
            pass = token.substr(6);
        }
        else if(lower.find("-id=") == 0){
            id = token.substr(4);
        }
    }

    if(user.empty() || pass.empty() || id.empty()){
        return "Error: login requiere -user -pass -id";
    }

    return execute(user,pass,id);
}

}

#endif