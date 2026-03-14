#ifndef MOUNTED_PARTITIONS_H
#define MOUNTED_PARTITIONS_H

#include <string>
#include <vector>

struct MountedPartition {
    std::string id;     
    std::string path;   
    std::string name;  
    int start;         
};

extern std::vector<MountedPartition> mountedPartitions;

#endif