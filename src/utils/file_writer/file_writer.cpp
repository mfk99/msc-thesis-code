#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
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

void writeResultsToFile(vector<long long> results)
{
    pugi::xml_document doc;
    pugi::xml_node dataNode = doc.append_child("data");
    for (size_t i = 0; i < results.size(); i++)
    {
        pugi::xml_node entryNode = dataNode.append_child("entry");
        entryNode.append_attribute("iteration") = to_string(i);
        entryNode.append_attribute("duration(µs)") = to_string(results[i]);
    }
    string fileName = getTimeStamp() + ".xml";
    doc.save_file(fileName.c_str());
}