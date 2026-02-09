#pragma once

#include <vector>
#include <map>
#include <string>

struct Room
{
    std::string id;
    int penalty;
};

struct Timing
{
    std::string days;
    std::string weeks;
    int start;
    int length;
    int penalty;
};

struct Class
{
    std::vector<Room> rooms;
    std::vector<Timing> timings;
};

struct Distribution
{
    bool required;
    int penalty;
    std::vector<std::string> classes;
};

void generateConfig(std::vector<int> configVariables);
std::vector<int> getConfigVariables();
std::vector<std::vector<std::string>> getCourseNames();
std::map<std::string, Class> getClasses();
std::vector<std::vector<std::vector<std::vector<int>>>> getRoomAvailability();
std::map<std::string, std::vector<Distribution>> getDistributions();