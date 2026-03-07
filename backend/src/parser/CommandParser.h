#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <string>
#include <sstream>
#include <algorithm>

#include "../commands/mkdisk.h"
#include "../commands/rmdisk.h"
#include "../commands/fdisk.h"
#include "../commands/mount.h"
#include "../commands/mkfs.h"
#include "../users/Login.h"
#include "../users/Logout.h"

namespace CommandParser {

    /*
        Convierte una cadena a minusculas
    */
    inline std::string toLowerCase(std::string str){
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    /*
        Ejecuta un comando ingresado en consola
    */
    inline std::string execute(const std::string& commandLine){

        std::istringstream iss(commandLine);
        std::string cmd;

        iss >> cmd;

        cmd = toLowerCase(cmd);

        /*
            MKDISK
        */
        if(cmd == "mkdisk"){
            return CommandMkdisk::execute(commandLine);
        }

        /*
            RMDISK
        */
        if(cmd == "rmdisk"){
            return CommandRmdisk::execute(commandLine);
        }

        /*
            FDISK
        */
        if(cmd == "fdisk"){
            return CommandFdisk::executeFromLine(commandLine);
        }

        /*
            MOUNT
        */
        if(cmd == "mount"){
            return CommandMount::executeFromLine(commandLine);
        }

        /*
            MKFS
        */
        if(cmd == "mkfs"){
            return CommandMkfs::executeFromLine(commandLine);
        }

        /*
            LOGIN
        */
        if(cmd == "login"){
            return CommandLogin::executeFromLine(commandLine);
        }

        /*
            LOGOUT
        */
        if(cmd == "logout"){
            return CommandLogout::execute();
        }

        /*
            EXIT
        */
        if(cmd == "exit"){
            return "EXIT";
        }

        return "Error: comando no reconocido";
    }

}

#endif