#pragma once

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include "./DistributionTypes.h"
#include "./ClassTypes.h"
#include "../../encoding/ipamir_utils/ipamir_clause_builder.h"

using namespace std;

struct OptimizationCriteria
{
    map<ClauseType, uint16_t> multiplierMap;
};

extern OptimizationCriteria optimization;
extern string problemName;

void generateConfig(vector<int> configVariables);
vector<int> getConfigVariables();
vector<vector<string>> getCourseNames();
map<string, Class> getClasses();
vector<Student> getStudents();
vector<HierarchyCourse> getClassHierarchy();
vector<vector<vector<vector<int>>>> getRoomAvailability();
map<string, vector<DistributionVariant>> getDistributions();