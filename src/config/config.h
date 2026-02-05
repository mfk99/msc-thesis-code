#pragma once

#include <vector>
#include <string>

struct Distribution
{
    bool required;
    int weight;
    std::vector<std::string> classes;
};

void generateConfig(std::vector<int> configVariables);
std::vector<int> getConfigVariables();
std::vector<std::vector<std::string>> getCourseNames();
std::vector<std::vector<std::vector<std::vector<int>>>> getRoomAvailability();
std::map<std::string, std::vector<Distribution>> getDistributions();