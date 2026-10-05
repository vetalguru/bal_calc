#include <iostream>
#include <string>
#include <vector>

#include "cli_app.h"

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return balcli::Run(args, std::cout, std::cerr);
}
