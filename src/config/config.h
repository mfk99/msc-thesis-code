#pragma once

#include <vector>
#include <map>
#include <string>
#include "./DistributionTypes.h"
#include "./ClassTypes.h"

using namespace std;

void generateConfig(vector<int> configVariables);
vector<int> getConfigVariables();
vector<vector<string>> getCourseNames();
map<string, Class> getClasses();
vector<vector<vector<vector<int>>>> getRoomAvailability();
map<string, vector<DistributionVariant>> getDistributions();