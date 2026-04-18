#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
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

void writeResultsToFile(vector<Result> results)
{
    pugi::xml_document doc;
    pugi::xml_node dataNode = doc.append_child("data");
    for (size_t i = 0; i < results.size(); i++)
    {
        pugi::xml_node entryNode = dataNode.append_child("entry");
        Result *result = &results[i];
        entryNode.append_attribute("iteration") = to_string(i);
        entryNode.append_attribute("durationMs") = to_string(result->solveTimeMs);
        entryNode.append_attribute("satisfied") = to_string(result->satisfied);
        entryNode.append_attribute("penalty") = to_string(result->penalty);
    }
    string fileName = getTimeStamp() + ".xml";
    doc.save_file(fileName.c_str());
}