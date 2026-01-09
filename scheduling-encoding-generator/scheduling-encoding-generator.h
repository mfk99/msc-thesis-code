#pragma once

#include <string>
#include <vector>

void generateEncoding(std::vector<int> generationVariables);
std::string getFilePath();
void printEncoding(long long vars, long long clauseCount,
                   std::vector<std::vector<int>> mustHaveRoomClauses,
                   std::vector<std::vector<int>> atMostOneClauses,
                   std::vector<std::vector<int>> roomConflictClauses);
void writeEncodingToFile(std::string encodingFilePath, long long vars, long long clauseCount,
                         std::vector<std::vector<int>> mustHaveRoomClauses,
                         std::vector<std::vector<int>> atMostOneClauses,
                         std::vector<std::vector<int>> roomConflictClauses,
                         std::vector<int> generationVariables);