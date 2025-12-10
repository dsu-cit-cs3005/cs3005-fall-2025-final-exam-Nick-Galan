#include "RobotBase.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>

class Robot_Sentinel : public RobotBase
{
private:
    int radar_dir = 1;
    bool enemy_seen = false;
    int enemy_row = -1;
    int enemy_col = -1;

public:
    Robot_Sentinel() : RobotBase(2, 5, hammer)
    {
        m_name = "Sentinel";
        m_character = 'H';
        std::srand(std::time(nullptr));
    }

    void get_radar_direction(int& dir) override
    {
        dir = radar_dir;
        radar_dir++;
        if (radar_dir > 8)
            radar_dir = 1;
    }

    void process_radar_results(const std::vector<RadarObj>& radar) override
    {
        enemy_seen = false;
        for (const auto& obj : radar)
        {
            if (obj.m_type == 'R')
            {
                enemy_seen = true;
                enemy_row = obj.m_row;
                enemy_col = obj.m_col;
                return;
            }
        }
    }

    bool get_shot_location(int& row, int& col) override
    {
        if (!enemy_seen)
            return false;

        int my_r, my_c;
        get_current_location(my_r, my_c);

        int drow = std::abs(enemy_row - my_r);
        int dcol = std::abs(enemy_col - my_c);

        if (drow <= 1 && dcol <= 1 && (drow + dcol > 0))
        {
            row = enemy_row;
            col = enemy_col;
            return true;
        }

        return false;
    }

    void get_move_direction(int& direction, int& distance) override
    {
        distance = 1;

        int my_r, my_c;
        get_current_location(my_r, my_c);

        if (!enemy_seen)
        {
            direction = (std::rand() % 8) + 1;
            return;
        }

        int dr = enemy_row - my_r;
        int dc = enemy_col - my_c;

        if (dr < 0 && dc == 0) direction = 1;
        else if (dr < 0 && dc > 0) direction = 2;
        else if (dr == 0 && dc > 0) direction = 3;
        else if (dr > 0 && dc > 0) direction = 4;
        else if (dr > 0 && dc == 0) direction = 5;
        else if (dr > 0 && dc < 0) direction = 6;
        else if (dr == 0 && dc < 0) direction = 7;
        else if (dr < 0 && dc < 0) direction = 8;
        else direction = 0;
    }
};

extern "C" RobotBase* create_robot()
{
    return new Robot_Sentinel();
}
