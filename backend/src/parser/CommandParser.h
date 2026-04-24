#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

/**
 * CommandParser.h — P2
 * ----------------------------------------------------------
 * Registra todos los comandos del P1 y los nuevos del P2.
 * ----------------------------------------------------------
 */

#include <string>
#include <sstream>
#include <algorithm>

// ── P1 (sin cambios) ──────────────────────────────────────
#include "../commands/mkdisk.h"
#include "../commands/rmdisk.h"
#include "../commands/fdisk.h"       // P2: agrega -add y -delete
#include "../commands/mount.h"
#include "../commands/mkfs.h"        // P2: agrega -fs=2fs|3fs
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

// ── P2 nuevos ─────────────────────────────────────────────
#include "../commands/unmount.h"
#include "../commands/journaling.h"
#include "../commands/new_commands.h"  // remove,rename,copy,move,find,chown,chmod,loss

namespace CommandParser {

inline std::string toLowerCase(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

inline std::string execute(const std::string& commandLine) {

    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;
    cmd = toLowerCase(cmd);

    // ── P1 ──────────────────────────────────────────────────
    if (cmd == "mkdisk")  return CommandMkdisk::execute(commandLine);
    if (cmd == "rmdisk")  return CommandRmdisk::execute(commandLine);
    if (cmd == "fdisk")   return CommandFdisk::executeFromLine(commandLine);
    if (cmd == "mount")   return CommandMount::executeFromLine(commandLine);
    if (cmd == "mounted") return CommandMounted::execute();
    if (cmd == "mkfs")    return CommandMkfs::executeFromLine(commandLine);
    if (cmd == "cat")     return CommandCat::execute(commandLine);
    if (cmd == "login")   return CommandLogin::executeFromLine(commandLine);
    if (cmd == "logout")  return CommandLogout::execute();
    if (cmd == "mkgrp")   return CommandMkgrp::executeFromLine(commandLine);
    if (cmd == "rmgrp")   return CommandRmgrp::executeFromLine(commandLine);
    if (cmd == "mkusr")   return CommandMkusr::executeFromLine(commandLine);
    if (cmd == "rmusr")   return CommandRmusr::executeFromLine(commandLine);
    if (cmd == "chgrp")   return CommandChgrp::executeFromLine(commandLine);
    if (cmd == "mkdir")   return CommandMkdir::executeFromLine(commandLine);
    if (cmd == "mkfile")  return CommandMkfile::executeFromLine(commandLine);
    if (cmd == "rep")     return CommandRep::executeFromLine(commandLine);

    // ── P2 nuevos ────────────────────────────────────────────
    if (cmd == "unmount")    return CommandUnmount::executeFromLine(commandLine);
    if (cmd == "journaling") return CommandJournaling::executeFromLine(commandLine);
    if (cmd == "remove")     return CommandRemove::executeFromLine(commandLine);
    if (cmd == "rename")     return CommandRename::executeFromLine(commandLine);
    if (cmd == "copy")       return CommandCopy::executeFromLine(commandLine);
    if (cmd == "move")       return CommandMove::executeFromLine(commandLine);
    if (cmd == "find")       return CommandFind::executeFromLine(commandLine);
    if (cmd == "chown")      return CommandChown::executeFromLine(commandLine);
    if (cmd == "chmod")      return CommandChmod::executeFromLine(commandLine);
    if (cmd == "loss")       return CommandLoss::executeFromLine(commandLine);

    // ── EXIT ────────────────────────────────────────────────
    if (cmd == "exit") return "EXIT";

    return "Error: comando no reconocido '" + cmd + "'";
}

} // namespace CommandParser

#endif // COMMANDPARSER_H