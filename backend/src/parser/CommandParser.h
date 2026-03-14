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
#include "../commands/mkgrp.h"
#include "../commands/mkusr.h"
#include "../commands/rmusr.h"
#include "../commands/rmgrp.h"
#include "../commands/chgrp.h"
#include "../commands/mkdir.h"
#include "../commands/mkfile.h"
#include "../commands/cat.h"
#include "../commands/mounted.h"
#include "../commands/rep.h"
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
            MOUNTED
        */
        if(cmd == "mounted"){
            return CommandMounted::execute();
        }

        /*
            MKFS
        */
        if(cmd == "mkfs"){
            return CommandMkfs::executeFromLine(commandLine);
        }

        /*
            CAT
        */
        if(cmd == "cat"){
            return CommandCat::execute(commandLine);
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
            MKGRP
        */

        if(cmd == "mkgrp"){
            return CommandMkgrp::executeFromLine(commandLine);
        }

        /*
            RMGRP
        */

        if (cmd == "rmgrp"){
            return CommandRmgrp::executeFromLine(commandLine);
        }
        
        /*
            MKUSR
        */

        if(cmd == "mkusr"){
            return CommandMkusr::executeFromLine(commandLine);
        }   
        
        /*
            RMUSR
        */

        if(cmd == "rmusr"){
            return CommandRmusr::executeFromLine(commandLine);
        }


        /*
            CHGRP
        */
        if(cmd == "chgrp"){
            return CommandChgrp::executeFromLine(commandLine);
        }

        /*
            MKDIR
        */
        if(cmd == "mkdir"){
            return CommandMkdir::executeFromLine(commandLine);
        }


        /*
            MKFILE
        */
        if(cmd == "mkfile"){
            return CommandMkfile::executeFromLine(commandLine);
        }

        /*
            REP
        */
        if(cmd == "rep"){
            return CommandRep::executeFromLine(commandLine);
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