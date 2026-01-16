#include <vector>
#include <string>
#include <jsoncpp/json/json.h>
#include <fstream>
#include "../input-parser/input-parser.h"
#include <iostream>

using namespace std;

void generateConfig(vector<int> configVariables)
{
    Json::Value root;

    int weeks = configVariables[0];
    int days = configVariables[1];
    int hours = configVariables[2];
    int rooms = configVariables[3];
    int courses = configVariables[4];
    int courseHours = configVariables[5];

    root["weeks"] = weeks;
    root["days"] = days;
    root["hours"] = hours;
    root["rooms"] = rooms;
    root["courses"] = courses;
    root["courseHours"] = courseHours;

    Json::Value roomAvailability(Json::arrayValue);
    for (int i = 0; i < rooms; i++)
    {

        Json::Value plane(Json::arrayValue);
        for (int i2 = 0; i2 < days; i2++)
        {
            Json::Value row(Json::arrayValue);
            for (int i3 = 0; i3 < hours; i3++)
            {
                row.append(1);
            }
            plane.append(row);
        }
        roomAvailability.append(plane);
    }

    root["roomAvailability"] = roomAvailability;

    Json::Value roomTravelTime(Json::arrayValue);
    for (int i = 0; i < rooms; i++)
    {
        Json::Value row(Json::arrayValue);
        for (int i2 = 0; i2 < rooms; i2++)
        {
            row.append(0);
        }
        roomTravelTime.append(row);
    }

    root["roomTravelTime"] = roomTravelTime;

    string path = filePath;
    std::ofstream configFile(path);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "    ";
    Json::StreamWriter *jsonWriter = writer.newStreamWriter();
    jsonWriter->write(root, &configFile);
    configFile.close();
}

vector<int> getConfigVariables()
{
    string path = filePath;
    std::ifstream configFile(filePath, std::ifstream::binary);
    Json::Value config;
    configFile >> config;

    vector<int> configVariables;
    int weeks = config["weeks"].asInt();
    int days = config["days"].asInt();
    int hours = config["hours"].asInt();
    int rooms = config["rooms"].asInt();
    int courses = config["courses"].asInt();
    int courseHours = config["courseHours"].asInt();
    configVariables.push_back(weeks);
    configVariables.push_back(days);
    configVariables.push_back(hours);
    configVariables.push_back(rooms);
    configVariables.push_back(courses);
    configVariables.push_back(courseHours);
    return configVariables;
}

vector<vector<vector<int>>> getRoomAvailability()
{
    string path = filePath;
    std::ifstream configFile(filePath, std::ifstream::binary);
    Json::Value config;
    configFile >> config;

    vector<vector<vector<int>>> roomAvailability;
    const Json::Value &jsonArray = config["roomAvailability"];
    for (const Json::Value &plane : jsonArray)
    {
        vector<vector<int>> room;
        for (const Json::Value &row : plane)
        {
            vector<int> dayHours;
            for (const Json::Value &hour : row)
            {
                dayHours.push_back(hour.asInt());
            }
            room.push_back(dayHours);
        }
        roomAvailability.push_back(room);
    }
    return roomAvailability;
}