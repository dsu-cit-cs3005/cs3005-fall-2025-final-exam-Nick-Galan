// RobotWarz.cpp
#include "Arena.h"
#include <iostream>

int main() {
    // For now, hard-code a 20x20 arena.
    // Later you can read these from a config file.
    int rows = 20;
    int cols = 20;

    try {
        Arena arena(rows, cols);
        arena.loadConfig("config.txt");   // TODO: create / adjust, or stub out
        arena.loadRobots();               // compile + dlopen + create robots
        arena.run();                      // main game loop
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
