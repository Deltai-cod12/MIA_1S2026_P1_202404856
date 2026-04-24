#ifndef MOUNTED_PARTITIONS_H
#define MOUNTED_PARTITIONS_H

#include <string>
#include <vector>

struct MountedPartition {
    std::string id;    // ID de montaje ej: "341A"
    std::string path;  // Ruta del disco ej: "/home/disco.mia"
    std::string name;  // Nombre de la partición
    int         start; // Byte donde inicia la partición
    int         size;  // Tamaño en bytes (requerido para mkfs P2)
};

extern std::vector<MountedPartition> mountedPartitions;

#endif