#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <memory>

#include "RobotBase.h"
#include "RadarObj.h"

struct RobotInfo {
    RobotBase* robot   = nullptr;
    void*      soHandle = nullptr;

    std::string name;
    char symbol = '?';

    int row = 0;
    int col = 0;

    bool alive = true;
    bool inPit = false;
};

class Arena {
public:
    Arena(int rows, int cols);

    // Load configuration (arena size, obstacles, max rounds, watchLive)
    void loadConfig(const std::string& filename);

    // Compile & load Robot_*.cpp files, create RobotBase instances
    void loadRobots();

    // Run the simulation until winner or max rounds
    void run();

private:
    int m_rows;
    int m_cols;

    // Simulation settings
    int  m_numMounds  = 5;
    int  m_numPits    = 3;
    int  m_numFlamers = 3;
    int  m_maxRounds  = 200;
    bool m_watchLive  = true;

    std::vector<std::vector<char>> m_board;
    std::vector<RobotInfo>         m_robots;

    // Setup helpers
    void initBoard();
    void placeObstacles();
    void placeRobotsRandomly();

    // Main loop helpers
    void printBoard(int round) const;
    void printRobotStatus(const RobotInfo& info) const;

    void runRound(int round);
    void handleRobotTurn(RobotInfo& info);

    bool isGameOver() const;
    int  countAliveRobots() const;
    int  getWinnerIndex() const;

    // Radar / movement / shooting
    std::vector<RadarObj> makeRadar(const RobotInfo& info, int radarDirection) const;

    void handleShot(RobotInfo& shooter, int shotRow, int shotCol);
    void handleMovement(RobotInfo& info, int moveDirection, int distance);

    void applyWeaponDamage(RobotInfo& target, WeaponType weapon);
    void applyFlameTrapDamage(RobotInfo& target);

    // Utilities
    bool inBounds(int r, int c) const;
    bool cellHasRobot(int r, int c, int& robotIndexOut) const;

    char symbolForRobot(size_t index) const;
};
