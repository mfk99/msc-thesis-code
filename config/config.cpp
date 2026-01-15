#include <vector>
#include <string>
#include <jsoncpp/json/json.h>
#include <fstream>
#include "../input-parser/input-parser.h"
#include <iostream>

using namespace std;

void generateConfig(vector<int> configVariables)
{
    Json::Value configData;
    int rooms = configVariables[0];
    int courseHours = configVariables[1];
    int courses = configVariables[2];
    int days = configVariables[3];
    int hours = configVariables[4];

    configData["rooms"] = rooms;
    configData["courseHours"] = courseHours;
    configData["courses"] = courses;
    configData["days"] = days;
    configData["hours"] = hours;

    string path = filePath;
    std::ofstream configFile(path);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "    ";
    Json::StreamWriter *jsonWriter = writer.newStreamWriter();
    jsonWriter->write(configData, &configFile);
    configFile.close();
}

vector<int> readConfig()
{
    string path = filePath;
    std::ifstream configFile(filePath, std::ifstream::binary);
    Json::Value config;
    configFile >> config;
    cout << config << "\n";

    vector<int> configVariables;
    int days = config["days"].asInt();
    int hours = config["hours"].asInt();
    int rooms = config["rooms"].asInt();
    int courses = config["courses"].asInt();
    int courseHours = config["courseHours"].asInt();
    configVariables.push_back(days);
    configVariables.push_back(hours);
    configVariables.push_back(rooms);
    configVariables.push_back(courses);
    configVariables.push_back(courseHours);
    return configVariables;
}