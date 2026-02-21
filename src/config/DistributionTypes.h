#pragma once

#include <vector>
#include <map>
#include <string>
#include <variant>

using namespace std;

struct Distribution
{
    bool required;
    int penalty;
    vector<string> classes;

    Distribution(bool required, int p, vector<string> c)
        : required(required), penalty(p), classes(move(c)) {}
};

struct WorkDayDistribution : public Distribution
{
    // Maximum length of a day (time betweeen the beginning of the first class and enc of second class)
    int S;
    WorkDayDistribution(bool required, int p, vector<string> c, int s)
        : Distribution(required, p, std::move(c)), S(s) {}
};

struct MinGapDistribution : public Distribution
{
    // Minimum length between classes
    int G;
    MinGapDistribution(bool required, int p, vector<string> c, int g)
        : Distribution(required, p, std::move(c)), G(g) {}
};

struct MaxDaysDistribution : public Distribution
{
    // Classes can be assigned to at most D days
    int D;
    MaxDaysDistribution(bool required, int p, vector<string> c, int d)
        : Distribution(required, p, std::move(c)), D(d) {}
};

struct MaxDayLoadDistribution : public Distribution
{
    // No more than S blocks can be assigned to classes on any day of the semester
    int S;
    MaxDayLoadDistribution(bool required, int p, vector<string> c, int s)
        : Distribution(required, p, std::move(c)), S(s) {}
};

struct MaxBreaksDistribution : public Distribution
{
    // Max n.o. breaks
    int R;
    // Block is counted as break if block length is S+1
    int S;
    MaxBreaksDistribution(bool required, int p, vector<string> c, int r, int s)
        : Distribution(required, p, std::move(c)), R(r), S(s) {}
};

struct MaxBlockDistribution : public Distribution
{
    // Maximum length of a continuous block of 2 or more classes
    int M;
    // Break length limit for M to be recognized as a single block
    int S;
    MaxBlockDistribution(bool required, int p, vector<string> c, int m, int s)
        : Distribution(required, p, std::move(c)), M(m), S(s) {}
};

using DistributionVariant = std::variant<
    Distribution,
    WorkDayDistribution,
    MinGapDistribution,
    MaxDaysDistribution,
    MaxDayLoadDistribution,
    MaxBreaksDistribution,
    MaxBlockDistribution>;