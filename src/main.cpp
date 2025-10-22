#include <orbis/Pad.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <string>
#include "assert.h"
#include "logging.h"
#include "types.h"

int main(int argc, char** argv) {
    LOG_INFO("Starting homebrew");

    if (argc > 1) {
        LOG_INFO("This is the one in /data/homebrew, exiting. (we got {} arguments)", argc);
        sceSystemServiceLoadExec("EXIT", nullptr);
    }

    std::vector<std::string> args{};
    std::ifstream args_file("/data/homebrew/args.txt");
    std::string line;
    while(std::getline(args_file, line)) {
        args.push_back(line);
    }

    char** const final_argv = new char*[args.size() + 1];
    for (int i = 0; i < args.size(); ++i) {
        final_argv[i] = new char[args[i].size() + 1];
        strncpy(final_argv[i], args[i].c_str(), args[i].size() + 1);
    }
    final_argv[args.size()] = new char[1];
    final_argv[args.size()][0] = '\0';

    LOG_INFO("Starting eboot with arguments:");
    for (int i = 0; i < args.size(); ++i) {
        LOG_INFO("Arg {:02}: '{}'", i, final_argv[i]);
    }

    sceSystemServiceLoadExec("/data/homebrew/eboot.bin", (const char**)final_argv);
    return 0;
}
