#pragma once
#include <vector>
#include <string>

struct Options
{
    bool verbose = false;
    bool initialize = false;
    bool generate = false;
    bool execute = false;
    bool manual_input = false;
    int iterations;
    int testType;
    std::vector<int> generationVariables;
    std::string filePath;
};

extern Options opts;

void parseInput(int argc, char **argv);