#pragma once
#include <vector>

extern unsigned short verbose;
extern unsigned short generate;
extern unsigned short execute;

void parseInput(int argc, char **argv);
void printHelp();
std::vector<int> parseEncodingGenerationVariables(int argc, char **argv);