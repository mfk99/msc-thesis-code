#pragma once
#include <vector>
#include <string>

extern unsigned short verbose;
extern unsigned short initialize;
extern unsigned short generate;
extern unsigned short execute;
extern std::string filePath;

void parseInput(int argc, char **argv);
void printHelp();
std::vector<int> parseEncodingGenerationVariables(int argc, char **argv);