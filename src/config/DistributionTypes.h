#pragma once

#include <vector>
#include <map>
#include <string>

using namespace std;

struct Distribution
{
    bool required;
    int penalty;
    vector<string> classes;
};

struct WorkDayDistribution : public Distribution
{
    // Maximum length of a day (time betweeen the beginning of the first class and enc of second class)
    int S;
};

struct MinGapDistribution : public Distribution
{
    // Minimum length between classes
    int G;
};

struct MaxDaysDistribution : public Distribution
{
    // Classes can be assigned to at most D days
    int D;
};

struct MaxDayLoadDistribution : public Distribution
{
    // No more than S blocks can be assigned to classes on any day of the semester
    int S;
};

struct MaxBreaksDistribution : public Distribution
{
    // Max n.o. breaks
    int R;
    // Block is counted as break if block length is S+1
    int S;
};

struct MaxBlockDistribution : public Distribution
{
    // Maximum length of a continuous block of 2 or more classes
    int M;
    // Break length limit for M to be recognized as a single block
    int S;
};