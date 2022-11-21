// Test file to try loading a GOTO-model into memory and running a sample verification run on it.
#include <iostream>
#include <vector>

#include <util/exception_utils.h>

#include "api.h"

int main(int argc, char *argv[])
{
    try {
        std::cout << "Hello from API stub" << std::endl;

        // Convert argv to vector of strings for initialize_goto_model
        std::vector<std::string> arguments(argv + 1, argv + argc);

        // Initialise API dependencies and global configuration in one step.
        cbmc_api api;

        // Demonstrate the loading of a goto-model from the command line arguments
        api.load_model_from_files(arguments);

        std::cout << "Successfully initialised goto_model" << std::endl;
        return 0;
    } catch (const invalid_command_line_argument_exceptiont &e) {
        std::cout << e.what() << std::endl;
    }
}


