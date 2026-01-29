#pragma once

#include <vector>
#include <string>

void generateConfig(std::vector<int> configVariables);
std::vector<int> getConfigVariables();
std::vector<std::vector<std::string>> getCourseNames();
std::vector<std::vector<std::vector<std::vector<int>>>> getRoomAvailability();
