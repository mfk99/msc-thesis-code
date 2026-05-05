#include <vector>
#include <chrono>
#include <tuple>
#include <iomanip>
#include <sstream>
#include "../config/config.h"
#include "../../encoding/semantics/scheduling_semantics.h"
#include "../../../libs/pugixml/src/pugixml.hpp"

using namespace std;

string getTimeStamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

void writeResultsToFile(IterationResult iterationResults)
{
    pugi::xml_document doc;
    pugi::xml_node dataNode = doc.append_child("data");
    dataNode.append_attribute("solver") = getSolverSignature();
    dataNode.append_attribute("problem_name") = problemName;
    for (size_t i = 0; i < iterationResults.optimization.size(); i++)
    {
        tuple<string, uint16_t> optimizationTuple = iterationResults.optimization[i];
        dataNode.append_attribute(get<0>(optimizationTuple)) = get<1>(optimizationTuple);
    }
    for (size_t i = 0; i < iterationResults.results.size(); i++)
    {
        pugi::xml_node entryNode = dataNode.append_child("entry");
        Result *result = &iterationResults.results[i];
        entryNode.append_attribute("iteration") = to_string(i);
        entryNode.append_attribute("duration_ms") = to_string(result->solveTimeMs);
        entryNode.append_attribute("satisfied") = to_string(result->satisfied);
        entryNode.append_attribute("penalty") = to_string(result->penalty);
    }
    string fileName = getTimeStamp() + ".xml";
    doc.save_file(fileName.c_str());
}

void writeMultipleIterationResultsToFile(vector<IterationResult> results)
{
    pugi::xml_document doc;
    pugi::xml_node dataNode = doc.append_child("data");
    dataNode.append_attribute("solver") = getSolverSignature();
    for (size_t i = 0; i < results.size(); i++)
    {
        pugi::xml_node iterNode = dataNode.append_child("iteration");
        IterationResult iterResult = results[i];
        for (tuple<string, uint16_t> optimizationTuple : iterResult.optimization)
        {
            iterNode.append_attribute(get<0>(optimizationTuple)) = get<1>(optimizationTuple);
        }
        for (size_t j = 0; j < iterResult.results.size(); j++)
        {
            pugi::xml_node entryNode = iterNode.append_child("entry");
            Result *result = &iterResult.results[j];
            entryNode.append_attribute("iteration") = to_string(j + 1);
            entryNode.append_attribute("duration_ms") = to_string(result->solveTimeMs);
            entryNode.append_attribute("satisfied") = to_string(result->satisfied);
            entryNode.append_attribute("penalty") = to_string(result->penalty);
        }
    }
    string fileName = getTimeStamp() + ".xml";
    doc.save_file(fileName.c_str());
}