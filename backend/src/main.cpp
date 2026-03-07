#include <iostream>
#include <string>
#include "parser/CommandParser.h"

int main() {

    std::string command;

    while(true){

        std::cout << ">> ";
        std::getline(std::cin, command);

        std::string result = CommandParser::execute(command);

        if(result == "EXIT")
            break;

        std::cout << result << std::endl;
    }

    return 0;
}