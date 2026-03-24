#pragma once

#include <vector>
#include <map>
#include <string>

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
    int capacity;
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

struct Student
{
    string id;
    vector<string> classIds;
};

struct HierarchyClass
{
    string id;
    string parentId;
    int limit;
};

struct HierarchySubpart
{
    string id;
    vector<HierarchyClass> classes;
};

struct HierarchyConfiguration
{
    string id;
    vector<HierarchySubpart> subparts;
};

struct HierarchyCourse
{
    string id;
    vector<HierarchyConfiguration> configs;
};

struct StudentCluster
{
    int id;
    vector<Student> students;
};

struct DecisionVar
{
    int literal;
    string classId;
    string configId;
    StudentCluster cluster;
};

struct ConfDecisionVar
{
    int literal;
    int clusterId;
    string configId;
};

struct StudentSectioningData
{
    vector<vector<DecisionVar>> s;
    vector<ConfDecisionVar> conf;
};