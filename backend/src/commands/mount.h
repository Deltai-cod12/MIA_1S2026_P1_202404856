#ifndef MOUNT_H
#define MOUNT_H

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdlib>

#include "../models/structs.h"
#include "../models/mounted_partitions.h"

namespace CommandMount {

     
    // Estructura para almacenar información de una partición montada
    
    struct MountedPartition {
        std::string path;
        std::string name;
        std::string id;
        char type;
        int start;
        int size;
    };

    
    // Mapa de particiones montadas
    // key = id (vda1, vdb1...)
    
    static std::map<std::string, MountedPartition> mountedPartitions;

      // Mapa de discos montados
        // key = path
        // value = letra asignada
    
    static std::map<std::string, char> diskLetters;

    
        // Siguiente letra disponible
    
    static char nextDiskLetter = 'a';


    
        // Expande rutas con ~ al home
    
    inline std::string expandPath(const std::string& path) {

        if (path.empty() || path[0] != '~')
            return path;

        const char* home = std::getenv("HOME");

        if (!home)
            home = std::getenv("USERPROFILE");

        if (home)
            return std::string(home) + path.substr(1);

        return path;
    }


    
        // Limpia espacios y caracteres nulos en nombres
    
    inline std::string cleanName(char name[16]) {

        std::string result(name);

        result.erase(
            std::remove_if(result.begin(), result.end(), ::isspace),
            result.end()
        );

        result.erase(
            std::find(result.begin(), result.end(), '\0'),
            result.end()
        );

        return result;
    }


    
       // Busca una partición dentro del MBR
    
    inline bool findPartitionInMBR(
        const std::string& path,
        const std::string& name,
        char& type,
        int& start,
        int& size
    ) {

        std::ifstream file(path, std::ios::binary);

        if (!file.is_open())
            return false;

        MBR mbr;
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        for (int i = 0; i < 4; i++) {

            if (mbr.mbr_partitions[i].part_status != '1')
                continue;

            std::string partName = cleanName(mbr.mbr_partitions[i].part_name);

            if (partName == name) {

                type = mbr.mbr_partitions[i].part_type;
                start = mbr.mbr_partitions[i].part_start;
                size = mbr.mbr_partitions[i].part_s;

                file.close();
                return true;
            }

            
               // Buscar particiones lógicas
            
            if (mbr.mbr_partitions[i].part_type == 'E') {

                int ebrPos = mbr.mbr_partitions[i].part_start;

                while (ebrPos != -1) {

                    EBR ebr;

                    file.seekg(ebrPos, std::ios::beg);
                    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

                    if (ebr.part_status == '1') {

                        std::string ebrName = cleanName(ebr.part_name);

                        if (ebrName == name) {

                            type = 'L';
                            start = ebr.part_start;
                            size = ebr.part_size;

                            file.close();
                            return true;
                        }
                    }

                    ebrPos = ebr.part_next;
                }
            }
        }

        file.close();
        return false;
    }


    
      //  Verifica si la partición ya está montada
    
    inline bool isPartitionMounted(
        const std::string& path,
        const std::string& name
    ) {

        for (const auto& [id, partition] : mountedPartitions) {

            if (partition.path == path && partition.name == name)
                return true;
        }

        return false;
    }


    
       // Genera el ID de montaje
       // formato: vd + letra + numero
    
    inline std::string generateMountID(const std::string& path) {

        const std::string carnet = "56";   // últimos dos dígitos de tu carnet

        char diskLetter;

        auto it = diskLetters.find(path);

        if (it != diskLetters.end()) {
            diskLetter = it->second;
        } else {
            diskLetter = nextDiskLetter++;
            diskLetters[path] = diskLetter;
        }

        int partitionNumber = 1;

        for (const auto& [id, partition] : mountedPartitions) {
            if (partition.path == path) {
                partitionNumber++;
            }
        }

        std::ostringstream mountID;

        mountID << carnet
                << partitionNumber
                << (char)toupper(diskLetter);

        return mountID.str();
    }

    
       // Ejecuta el comando mount
    
    inline std::string execute(
        const std::string& pathParam,
        const std::string& nameParam
    ) {

        std::string path = expandPath(pathParam);
        std::string name = nameParam;

        std::ifstream file(path);

        if (!file.good())
            return "Error: el disco '" + path + "' no existe";

        file.close();

        if (isPartitionMounted(path, name))
            return "Error: la partición '" + name + "' ya está montada";

        char type;
        int start;
        int size;

        if (!findPartitionInMBR(path, name, type, start, size))
            return "Error: no se encontró la partición '" + name + "'";

        std::string mountID = generateMountID(path);

        std::fstream diskFile(path, std::ios::in | std::ios::out | std::ios::binary);
        if (diskFile.is_open()) {
            MBR mbr;
            diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

            bool found = false;
            for (int i = 0; i < 4; i++) {
                std::string currentName = cleanName(mbr.mbr_partitions[i].part_name);
                if (currentName == name) {
                    // Guardamos el ID generado en el struct del MBR
                    memset(mbr.mbr_partitions[i].part_id, 0, 4);
                    strncpy(mbr.mbr_partitions[i].part_id, mountID.c_str(), 4);
                    found = true;
                    break;
                }
            }

            if (found) {
                diskFile.seekp(0, std::ios::beg);
                diskFile.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            }
            diskFile.close();
        }

        MountedPartition mounted;

        mounted.path = path;
        mounted.name = name;
        mounted.id = mountID;
        mounted.type = type;
        mounted.start = start;
        mounted.size = size;

        mountedPartitions[mountID] = mounted;

        ::MountedPartition globalMount;

        globalMount.id = mountID;
        globalMount.path = path;
        globalMount.name = name;
        globalMount.start = start;

        ::mountedPartitions.push_back(globalMount);

        std::ostringstream result;

        result << "\n=== MOUNT ===\n";
        result << "Partición montada exitosamente\n";
        result << "ID: " << mountID << "\n";
        result << "Disco: " << path << "\n";
        result << "Partición: " << name << "\n";
        result << "Tipo: " << type << "\n";
        result << "Inicio: " << start << "\n";
        result << "Tamaño: " << size << " bytes";

        return result.str();
    }


    
      //  Lista particiones montadas
    
    inline std::string listMountedPartitions() {

        if (mountedPartitions.empty())
            return "No hay particiones montadas";

        std::ostringstream result;

        result << "\n=== PARTICIONES MONTADAS ===\n";

        for (const auto& [id, partition] : mountedPartitions) {

            result << "ID: " << id << "\n";
            result << "Disco: " << partition.path << "\n";
            result << "Partición: " << partition.name << "\n";
            result << "Tipo: " << partition.type << "\n";
            result << "Inicio: " << partition.start << "\n";
            result << "Tamaño: " << partition.size << " bytes\n";
            result << "-----------------\n";
        }

        return result.str();
    }


    
    //  UNMOUNT (NUEVO - P2)
    inline void unmountPartition(const std::string& id) {

        // eliminar del map
        mountedPartitions.erase(id);

        // eliminar del vector global
        ::mountedPartitions.erase(
            std::remove_if(
                ::mountedPartitions.begin(),
                ::mountedPartitions.end(),
                [&](const ::MountedPartition& p) {
                    return p.id == id;
                }
            ),
            ::mountedPartitions.end()
        );
    }



    
       // Obtener partición montada por ID
    
    inline bool getMountedPartition(
        const std::string& id,
        MountedPartition& partition
    ) {

        auto it = mountedPartitions.find(id);

        if (it == mountedPartitions.end())
            return false;

        partition = it->second;
        return true;
    }

    
   // Parsear linea completa del comando mount
    
    inline std::string executeFromLine(const std::string& commandLine){

        std::istringstream iss(commandLine);
        std::string token;

        std::string path;
        std::string name;

        while(iss >> token){

            std::string lower = token;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if(lower.find("-path=") == 0){
                path = token.substr(6);
            }
            else if(lower.find("-name=") == 0){
                name = token.substr(6);
            }
        }

        if(path.empty() || name.empty()){
            return "Error: mount requiere -path y -name";
        }

        return execute(path,name);
    }

}

#endif