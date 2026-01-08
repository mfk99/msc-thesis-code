#pragma once

#include <vector>
#include <string>

std::vector<int> parseUserClauseInput(std::string input);
std::vector<int> parseGenerationVariablesFromFile(std::string filePath);
void runBenchMark(std::string encodingFilePath);