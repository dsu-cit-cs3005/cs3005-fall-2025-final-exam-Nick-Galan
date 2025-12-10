# Compiler
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -fPIC

# All robot source files automatically detected
ROBOT_SRCS := $(wildcard Robot_*.cpp)
ROBOT_LIBS := $(ROBOT_SRCS:.cpp=.so)

# Targets
all: RobotWarz test_robot

RobotWarz: RobotWarz.cpp Arena.o RobotBase.o
	$(CXX) $(CXXFLAGS) RobotWarz.cpp Arena.o RobotBase.o -ldl -pthread -o RobotWarz

Arena.o: Arena.cpp Arena.h
	$(CXX) $(CXXFLAGS) -c Arena.cpp

RobotBase.o: RobotBase.cpp RobotBase.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp

test_robot: test_robot.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) test_robot.cpp RobotBase.o -ldl -o test_robot

# Build shared libraries for robots
%.so: %.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) -shared -o $@ $< RobotBase.o

# Clean up
clean:
	rm -f *.o *.so RobotWarz test_robot
