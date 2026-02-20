#pragma once

#include <vector>
#include <map>
#include <string>
#include "./DistributionTypes.h"

using namespace std;

struct RoomUnavailability
{
    string days;
    string weeks;
    int start;
    int length;
};

struct Room
{
    string id;
    int penalty;
    vector<RoomUnavailability> unavailability;
    map<string, int> travelTimes;
};

struct Timing
{
    string days;
    string weeks;
    int start;
    int length;
    int penalty;
};

struct Class
{
    string id;
    vector<Room> rooms;
    vector<Timing> timings;
};

void generateConfig(vector<int> configVariables);
vector<int> getConfigVariables();
vector<vector<string>> getCourseNames();
map<string, Class> getClasses();
vector<vector<vector<vector<int>>>> getRoomAvailability();
map<string, vector<DistributionVariant>> getDistributions();