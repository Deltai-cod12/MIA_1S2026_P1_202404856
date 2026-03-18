#ifndef COMMAND_MOUNTED_H
#define COMMAND_MOUNTED_H

#include <sstream>
#include <iostream>
#include "../models/mounted_partitions.h"

// Comando para ver las particiones montadas

namespace CommandMounted {

    inline std::string execute(){

        std::stringstream output;

        if(mountedPartitions.empty()){
            output << "No hay particiones montadas";
            return output.str();
        }

        output << "Particiones montadas:\n";

        for(size_t i = 0; i < mountedPartitions.size(); i++){
            output << mountedPartitions[i].id;

            if(i < mountedPartitions.size() - 1)
                output << ", ";
        }

        return output.str();
    }

}

#endif