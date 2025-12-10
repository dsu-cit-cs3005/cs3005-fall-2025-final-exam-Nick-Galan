#include "Arena.h"

#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>
#include <stdexcept>
#include <cmath>

namespace fs = std::filesystem;

namespace {
int directionFromDelta(int dr, int dc)
{
    if (dr == 0 && dc == 0) return 0;

    if (dr < 0 && dc == 0)  return 1; // Up
    if (dr < 0 && dc > 0)   return 2; // Up-right
    if (dr == 0 && dc > 0)  return 3; // Right
    if (dr > 0 && dc > 0)   return 4; // Down-right
    if (dr > 0 && dc == 0)  return 5; // Down
    if (dr > 0 && dc < 0)   return 6; // Down-left
    if (dr == 0 && dc < 0)  return 7; // Left
    if (dr < 0 && dc < 0)   return 8; // Up-left

    return 0;
}
}

Arena::Arena(int rows, int cols)
    : m_rows(rows),
      m_cols(cols),
      m_board(rows, std::vector<char>(cols, '.'))
{
    if (rows < 10 || cols < 10) {
        throw std::runtime_error("Arena must be at least 10x10.");
    }

    initBoard();
}

void Arena::loadConfig(const std::string& /*filename*/) {

}

void Arena::initBoard() {
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            m_board[r][c] = '.';
        }
    }
    placeObstacles();
}

bool Arena::inBounds(int r, int c) const {
    return r >= 0 && r < m_rows && c >= 0 && c < m_cols;
}

void Arena::placeObstacles() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> rowDist(0, m_rows - 1);
    std::uniform_int_distribution<int> colDist(0, m_cols - 1);

    auto placeMany = [&](int count, char ch) {
        int placed = 0;
        while (placed < count) {
            int r = rowDist(rng);
            int c = colDist(rng);
            if (m_board[r][c] == '.') {
                m_board[r][c] = ch;
                ++placed;
            }
        }
    };

    placeMany(m_numMounds,  'M');
    placeMany(m_numPits,    'P');
    placeMany(m_numFlamers, 'F');
}

char Arena::symbolForRobot(size_t index) const {
    static const std::string symbols = "!@#$%^&*?";
    return symbols[index % symbols.size()];
}

void Arena::loadRobots() {
    std::cout << "Loading Robots...\n";

    std::vector<fs::path> robotSources;
    for (const auto& entry : fs::directory_iterator(".")) {
        if (!entry.is_regular_file()) continue;
        auto name = entry.path().filename().string();
        if (name.rfind("Robot_", 0) == 0 && name.size() > 10 && name.ends_with(".cpp")) {
            robotSources.push_back(entry.path());
        }
    }

    if (robotSources.empty()) {
        std::cout << "No Robot_*.cpp files found.\n";
    }

    for (size_t i = 0; i < robotSources.size(); ++i) {
        const auto& srcPath = robotSources[i];
        std::string filename  = srcPath.filename().string();

        std::string base      = srcPath.stem().string();
        std::string sharedLib = "lib" + base + ".so";

        std::string compileCmd =
            "g++ -shared -fPIC -o " + sharedLib + " " + filename +
            " RobotBase.o -I. -std=c++20";

        std::cout << "Compiling " << filename << " to " << sharedLib << "...\n";
        int compileResult = std::system(compileCmd.c_str());
        if (compileResult != 0) {
            std::cerr << "Failed to compile " << filename
                      << " with command: " << compileCmd << "\n";
            continue;
        }

        std::string soPath = "./" + sharedLib;
        void* handle = dlopen(soPath.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Failed to load " << soPath
                      << ": " << dlerror() << "\n";
            continue;
        }

        RobotFactory create_robot =
            (RobotFactory)dlsym(handle, "create_robot");
        if (!create_robot) {
            std::cerr << "Failed to find create_robot in " << soPath
                      << ": " << dlerror() << "\n";
            dlclose(handle);
            continue;
        }

        RobotBase* robot = create_robot();
        if (!robot) {
            std::cerr << "create_robot() returned nullptr for "
                      << soPath << "\n";
            dlclose(handle);
            continue;
        }

        robot->set_boundaries(m_rows, m_cols);

        RobotInfo info;
        info.robot    = robot;
        info.soHandle = handle;
        info.name     = robot->m_name;
        info.symbol   = symbolForRobot(i);
        info.alive    = true;
        info.inPit    = false;

        m_robots.push_back(info);
    }

    placeRobotsRandomly();
}

void Arena::placeRobotsRandomly() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> rowDist(0, m_rows - 1);
    std::uniform_int_distribution<int> colDist(0, m_cols - 1);

    for (auto& info : m_robots) {
        while (true) {
            int r = rowDist(rng);
            int c = colDist(rng);

            bool occupied = false;
            if (m_board[r][c] != '.') {
                occupied = true;
            } else {
                for (const auto& other : m_robots) {
                    if (&other == &info) continue;
                    if (other.row == r && other.col == c) {
                        occupied = true;
                        break;
                    }
                }
            }

            if (!occupied) {
                info.row = r;
                info.col = c;
                info.robot->move_to(r, c);
                break;
            }
        }
    }
}

void Arena::printBoard(int round) const {
    std::cout << "\n=========== starting round " << round << " ===========\n\n";

    std::cout << "   ";
    for (int c = 0; c < m_cols; ++c) {
        std::cout << std::setw(2) << c << " ";
    }
    std::cout << "\n";

    auto temp = m_board;

    for (const auto& info : m_robots) {
        char ch;
        if (!info.alive || info.robot->get_health() <= 0) {
            ch = 'X';
        } else {
            ch = info.symbol;
        }
        if (inBounds(info.row, info.col)) {
            temp[info.row][info.col] = ch;
        }
    }

    for (int r = 0; r < m_rows; ++r) {
        std::cout << std::setw(2) << r << " ";
        for (int c = 0; c < m_cols; ++c) {
            std::cout << " " << temp[r][c] << " ";
        }
        std::cout << "\n\n";
    }
}

void Arena::printRobotStatus(const RobotInfo& info) const {
    std::cout << info.name << " " << info.symbol << " begins turn.\n";
    std::cout << "  " << info.robot->print_stats() << "\n";
}

void Arena::run() {
    int round = 0;

    while (!isGameOver() && round < m_maxRounds) {
        printBoard(round);
        runRound(round);
        ++round;

        if (m_watchLive) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    int winner = getWinnerIndex();
    if (winner >= 0) {
        std::cout << "Game Over. Winner: "
                  << m_robots[winner].name
                  << " " << m_robots[winner].symbol << "\n";
    } else {
        std::cout << "Game Over. No winner (draw).\n";
    }
}

void Arena::runRound(int /*round*/) {
    for (auto& info : m_robots) {
        if (!info.alive || info.robot->get_health() <= 0) {
            continue;
        }

        printRobotStatus(info);

        int radarDir = 0;
        info.robot->get_radar_direction(radarDir);
        auto radarResults = makeRadar(info, radarDir);
        info.robot->process_radar_results(radarResults);

        int shotRow = 0;
        int shotCol = 0;
        bool willShoot = info.robot->get_shot_location(shotRow, shotCol);

        if (willShoot) {
            handleShot(info, shotRow, shotCol);
        } else {
            int moveDir = 0;
            int distance = 0;
            info.robot->get_move_direction(moveDir, distance);
            handleMovement(info, moveDir, distance);
        }

        std::cout << "\n";
    }
}

bool Arena::isGameOver() const {
    return countAliveRobots() <= 1;
}

int Arena::countAliveRobots() const {
    int count = 0;
    for (const auto& r : m_robots) {
        if (r.alive && r.robot->get_health() > 0) {
            ++count;
        }
    }
    return count;
}

int Arena::getWinnerIndex() const {
    int idx = -1;
    for (int i = 0; i < static_cast<int>(m_robots.size()); ++i) {
        if (m_robots[i].alive && m_robots[i].robot->get_health() > 0) {
            if (idx == -1) {
                idx = i;
            } else {
                return -1;
            }
        }
    }
    return idx;
}

bool Arena::cellHasRobot(int r, int c, int& robotIndexOut) const {
    for (int i = 0; i < static_cast<int>(m_robots.size()); ++i) {
        if (m_robots[i].alive &&
            m_robots[i].row == r &&
            m_robots[i].col == c) {
            robotIndexOut = i;
            return true;
        }
    }
    return false;
}

std::vector<RadarObj> Arena::makeRadar(const RobotInfo& info,
                                       int radarDirection) const {
    std::vector<RadarObj> results;

    int r0 = info.row;
    int c0 = info.col;

    auto addCell = [&](int r, int c) {
        if (!inBounds(r, c)) return;
        if (r == r0 && c == c0) return;

        char ch = m_board[r][c];

        for (const auto& rob : m_robots) {
            if (rob.row == r && rob.col == c) {
                if (!rob.alive || rob.robot->get_health() <= 0) {
                    ch = 'X';
                } else {
                    ch = 'R';
                }
                break;
            }
        }

        results.emplace_back(ch, r, c);
    };

    if (radarDirection == 0) {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;
                int rr = r0 + dr;
                int cc = c0 + dc;
                addCell(rr, cc);
            }
        }
        return results;
    }

    if (radarDirection < 1 || radarDirection > 8) {
        return results;
    }

    int dr = directions[radarDirection].first;
    int dc = directions[radarDirection].second;

    int pr = -dc;
    int pc = dr;

    int curRow = r0 + dr;
    int curCol = c0 + dc;

    while (inBounds(curRow, curCol)) {
        addCell(curRow, curCol);
        addCell(curRow + pr, curCol + pc);
        addCell(curRow - pr, curCol - pc);

        curRow += dr;
        curCol += dc;
    }

    return results;
}

void Arena::handleMovement(RobotInfo& info, int moveDirection, int distance) {
    if (info.inPit || info.robot->get_move_speed() == 0) {
        std::cout << "  " << info.name << " is stuck and cannot move.\n";
        return;
    }

    if (moveDirection < 1 || moveDirection > 8) {
        std::cout << "  " << info.name << " is not moving.\n";
        return;
    }

    int maxSpeed = info.robot->get_move_speed();
    if (distance > maxSpeed) {
        distance = maxSpeed;
    }
    if (distance <= 0) {
        std::cout << "  " << info.name << " is not moving.\n";
        return;
    }

    int curRow = info.row;
    int curCol = info.col;

    int dr = directions[moveDirection].first;
    int dc = directions[moveDirection].second;

    int startRow = curRow;
    int startCol = curCol;

    for (int step = 0; step < distance; ++step) {
        int nextRow = curRow + dr;
        int nextCol = curCol + dc;

        if (!inBounds(nextRow, nextCol)) {
            break;
        }

        bool blockedByRobot = false;
        for (const auto& other : m_robots) {
            if (&other == &info) continue;
            if (other.row == nextRow && other.col == nextCol) {
                blockedByRobot = true;
                break;
            }
        }
        if (blockedByRobot) {
            break;
        }

        char cell = m_board[nextRow][nextCol];

        if (cell == 'M') {
            break;
        } else if (cell == 'P') {
            curRow = nextRow;
            curCol = nextCol;
            info.row = curRow;
            info.col = curCol;
            info.robot->move_to(curRow, curCol);

            info.inPit = true;
            info.robot->disable_movement();

            std::cout << "  " << info.name << " falls into a pit at ("
                      << curRow << "," << curCol << ").\n";
            break;
        } else if (cell == 'F') {
            curRow = nextRow;
            curCol = nextCol;
            info.row = curRow;
            info.col = curCol;
            info.robot->move_to(curRow, curCol);

            std::cout << "  " << info.name << " moves through a flame trap at ("
                      << curRow << "," << curCol << ").\n";
            applyFlameTrapDamage(info);

            if (!info.alive || info.robot->get_health() <= 0) {
                break;
            }
        } else {
            curRow = nextRow;
            curCol = nextCol;
            info.row = curRow;
            info.col = curCol;
            info.robot->move_to(curRow, curCol);
        }
    }

    if (startRow != curRow || startCol != curCol) {
        std::cout << "  Moving: " << info.name << " moves to ("
                  << curRow << "," << curCol << ").\n";
    } else {
        std::cout << "  " << info.name << " stays at ("
                  << curRow << "," << curCol << ").\n";
    }
}

void Arena::applyFlameTrapDamage(RobotInfo& target) {
    applyWeaponDamage(target, flamethrower);
}

void Arena::handleShot(RobotInfo& shooter, int shotRow, int shotCol) {
    WeaponType weapon = shooter.robot->get_weapon();

    if (weapon == grenade) {
        if (shooter.robot->get_grenades() <= 0) {
            std::cout << "  " << shooter.name << " is out of grenades.\n";
            return;
        }
        shooter.robot->decrement_grenades();
    }

    int sr = shooter.row;
    int sc = shooter.col;

    auto damageAtCell = [&](int r, int c) {
        if (!inBounds(r, c)) return;
        for (auto& target : m_robots) {
            if (!target.alive || target.robot->get_health() <= 0) continue;
            if (target.row == r && target.col == c) {
                applyWeaponDamage(target, weapon);
            }
        }
    };

    int dr = shotRow - sr;
    int dc = shotCol - sc;
    int dirIndex = directionFromDelta(dr, dc);

    switch (weapon) {
    case railgun:
    {
        if (dirIndex == 0) {
            std::cout << "  " << shooter.name << " fires railgun but direction is invalid.\n";
            return;
        }

        int stepR = directions[dirIndex].first;
        int stepC = directions[dirIndex].second;

        std::cout << "  Shooting: railgun\n";

        int r = sr + stepR;
        int c = sc + stepC;
        while (inBounds(r, c)) {
            damageAtCell(r, c);
            r += stepR;
            c += stepC;
        }
        break;
    }

    case hammer:
    {
        if (dirIndex == 0) {
            std::cout << "  " << shooter.name << " swings hammer but hits nothing.\n";
            return;
        }

        int stepR = directions[dirIndex].first;
        int stepC = directions[dirIndex].second;

        int r = sr + stepR;
        int c = sc + stepC;

        std::cout << "  Shooting: hammer\n";
        damageAtCell(r, c);
        break;
    }

    case flamethrower:
    {
        if (dirIndex == 0) {
            std::cout << "  " << shooter.name << " fires flamethrower blindly.\n";
            return;
        }

        int stepR = directions[dirIndex].first;
        int stepC = directions[dirIndex].second;

        int pr = -stepC;
        int pc = stepR;

        std::cout << "  Shooting: flamethrower\n";

        for (int k = 1; k <= 4; ++k) {
            int centerR = sr + stepR * k;
            int centerC = sc + stepC * k;
            if (!inBounds(centerR, centerC)) break;

            damageAtCell(centerR, centerC);
            damageAtCell(centerR + pr, centerC + pc);
            damageAtCell(centerR - pr, centerC - pc);
        }
        break;
    }

    case grenade:
    {
        if (!inBounds(shotRow, shotCol)) {
            std::cout << "  " << shooter.name << " throws grenade off the board.\n";
            return;
        }

        std::cout << "  Shooting: grenade at (" << shotRow << "," << shotCol << ")\n";

        for (int r = shotRow - 1; r <= shotRow + 1; ++r) {
            for (int c = shotCol - 1; c <= shotCol + 1; ++c) {
                damageAtCell(r, c);
            }
        }
        break;
    }
    }
}

void Arena::applyWeaponDamage(RobotInfo& target, WeaponType weapon) {
    if (!target.alive) return;

    std::mt19937 rng(std::random_device{}());
    int minD = 0;
    int maxD = 0;

    switch (weapon) {
    case railgun:
        minD = 10; maxD = 20; break;
    case hammer:
        minD = 50; maxD = 60; break;
    case grenade:
        minD = 10; maxD = 40; break;
    case flamethrower:
        minD = 30; maxD = 50; break;
    }

    std::uniform_int_distribution<int> dist(minD, maxD);
    int rawDamage = dist(rng);

    int armor = target.robot->get_armor();
    double reduction = armor * 0.10;
    if (reduction > 0.9) reduction = 0.9;
    int finalDamage = static_cast<int>(rawDamage * (1.0 - reduction));

    if (armor > 0) {
        target.robot->reduce_armor(1);
    }

    int newHealth = target.robot->take_damage(finalDamage);
    std::cout << "  " << target.name << " takes "
              << finalDamage << " damage. Health: " << newHealth << "\n";

    if (newHealth <= 0) {
        target.alive = false;
        std::cout << "  " << target.name << " is out!\n";
    }
}
