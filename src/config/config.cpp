#include <vector>
#include <string>
#include <cstring>
#include "../../libs/pugixml-1.15/src/pugixml.hpp"
#include <fstream>
#include "../input-parser/input-parser.h"
#include <iostream>

using namespace std;
using namespace pugi;

/* The style of creating a configuration file
largely adheres to the ITC 2019 data format.
More info available at: https://www.itc2019.org/format
*/

void generateConfig(vector<int> configVariables)
{
    int weeks = configVariables[0];
    int days = configVariables[1];
    int hours = configVariables[2];
    int rooms = configVariables[3];
    int courses = configVariables[4];
    int courseHours = configVariables[5];

    xml_document doc;
    // add node with some name
    xml_node root = doc.append_child("problem");
    root.append_attribute("name") = filePath;
    root.append_attribute("nrDays") = days;
    root.append_attribute("nrWeeks") = weeks;
    root.append_attribute("slotsPerDay") = hours;

    // add description node with text child
    root.append_child("optimization");
    xml_node roomsXmlNode = root.append_child("rooms");
    xml_node coursesXmlNode = root.append_child("courses");
    xml_node distributionsXmlNode = root.append_child("distributions");
    root.append_child("students");

    for (int i = 0; i < rooms; i++)
    {
        xml_node singleRoomXmlNode = roomsXmlNode.append_child("room");
        singleRoomXmlNode.append_attribute("id") = i + 1;
        singleRoomXmlNode.append_attribute("capacity") = 0;
        xml_node UnavailabilityXmlNode = singleRoomXmlNode.append_child("unavailable");
        UnavailabilityXmlNode.append_attribute("days") = string(days, '0');
        UnavailabilityXmlNode.append_attribute("start") = "0";
        UnavailabilityXmlNode.append_attribute("length") = "0";
        UnavailabilityXmlNode.append_attribute("weeks") = string(weeks, '0');
    }

    // TODO: Add support for multiple configs and subparts
    int idCounter = 1;
    vector<vector<string>> classIds;
    for (int course = 1; course <= courses; course++)
    {
        vector<string> courseClassIds;
        xml_node singleCourseXmlNode = coursesXmlNode.append_child("course");
        singleCourseXmlNode.append_attribute("id") = course;
        for (int config = 0; config < 1; config++)
        {
            xml_node courseConfigXmlNode = singleCourseXmlNode.append_child("config");
            for (int subpart = 1; subpart <= 1; subpart++)
            {
                xml_node courseConfigSubPartXmlNode = courseConfigXmlNode.append_child("subpart");
                courseConfigSubPartXmlNode.append_attribute("id") = subpart;
                for (int lecture = 1; lecture <= courseHours; lecture++)
                {
                    xml_node lectureXmlNode = courseConfigSubPartXmlNode.append_child("class");
                    string classId = "Lec" + to_string(idCounter);
                    lectureXmlNode.append_attribute("id") = classId;
                    courseClassIds.push_back(classId);
                    idCounter++;
                    lectureXmlNode.append_attribute("limit") = 0;
                }
            }
        }
        classIds.push_back(courseClassIds);
    }

    // TODO: add maybe couple more, make the precedence one functional
    for (int course = 0; course < courses; course++)
    {
        xml_node precedenceXmlNode = distributionsXmlNode.append_child("distribution");
        precedenceXmlNode.append_attribute("type") = "Precedence";
        precedenceXmlNode.append_attribute("required") = "true";
        for (string classId : classIds[course])
        {
            xml_node classXmlNode = precedenceXmlNode.append_child("class");
            classXmlNode.append_attribute("id") = classId;
        }
    }

    char *filePathChar = new char[filePath.length() + 1];
    strcpy(filePathChar, filePath.c_str());
    doc.save_file(filePathChar);
}

vector<int> getConfigVariables()
{
    char *filePathChar = new char[filePath.length() + 1];
    strcpy(filePathChar, filePath.c_str());
    xml_document doc;
    xml_parse_result result = doc.load_file(filePathChar);
    if (verbose)
        cout << "[VERBOSE] Load result: " << result.description() << "\n";

    xml_node problemNode = doc.child("problem");
    int weeks = problemNode.attribute("nrWeeks").as_int();
    int days = problemNode.attribute("nrDays").as_int();
    int hours = problemNode.attribute("slotsPerDay").as_int();

    int rooms = 0;
    xml_node roomsNode = problemNode.child("rooms");
    for (xml_node roomNode : roomsNode.children())
        rooms++;

    int courses = 0;
    xml_node coursesNode = problemNode.child("courses");
    for (xml_node courseNode : coursesNode.children())
        courses++;

    int courseHours = 0;
    for (xml_node classNode : coursesNode.child("course").child("config").child("subpart").children())
        courseHours++;

    return vector<int>{weeks, days, hours, rooms, courses, courseHours};
}

vector<vector<string>> getCourseNames()
{
    char *filePathChar = new char[filePath.length() + 1];
    strcpy(filePathChar, filePath.c_str());
    xml_document doc;
    xml_parse_result result = doc.load_file(filePathChar);
    if (verbose)
        cout << "[VERBOSE] Load result: " << result.description() << "\n";

    xml_node coursesNode = doc.child("problem").child("courses");
    vector<vector<string>> classNames;
    for (xml_node courseNode : coursesNode.children())
    {
        vector<string> courseClassNames;
        for (xml_node courseClassNode : courseNode.child("config").child("subpart").children())
        {
            string courseNameStr = courseClassNode.attribute("id").as_string();
            courseClassNames.push_back(courseNameStr);
        }
        classNames.push_back(courseClassNames);
    }
    return (classNames);
}

vector<vector<vector<vector<int>>>> getRoomAvailability()
{
    char *filePathChar = new char[filePath.length() + 1];
    strcpy(filePathChar, filePath.c_str());
    xml_document doc;
    xml_parse_result result = doc.load_file(filePathChar);
    if (verbose)
        cout << "[VERBOSE] Load result: " << result.description() << "\n";

    xml_node problemNode = doc.child("problem");
    int weeks = problemNode.attribute("nrWeeks").as_int();
    int days = problemNode.attribute("nrDays").as_int();
    int hours = problemNode.attribute("slotsPerDay").as_int();
    xml_node roomsNode = problemNode.child("rooms");
    size_t rooms = distance(roomsNode.children("room").begin(), roomsNode.children("room").end());
    vector<vector<vector<vector<int>>>> roomAvailability(rooms, vector<vector<vector<int>>>(weeks, vector<vector<int>>(days, vector<int>(hours))));

    int roomIndex = 0;
    for (xml_node roomNode : roomsNode.children())
    {
        for (xml_node unavailabilityNode : roomNode.children("unavailable"))
        {
            string weeklyRoomUnavailability = unavailabilityNode.attribute("weeks").as_string();
            for (int weekIndex = 0; weekIndex < weeks; weekIndex++)
            {
                if (weeklyRoomUnavailability[weekIndex] == '1')
                {
                    string dailyRoomUnavailability = unavailabilityNode.attribute("days").as_string();
                    for (int dayIndex = 0; dayIndex < days; dayIndex++)
                    {
                        if (dailyRoomUnavailability[dayIndex] == '1')
                        {
                            int unavailabilityStartIndex = unavailabilityNode.attribute("start").as_int();
                            int unavailabilityLength = unavailabilityNode.attribute("length").as_int();
                            for (int i = 0; i < unavailabilityLength; i++)
                            {
                                roomAvailability[roomIndex]
                                                [weekIndex]
                                                [dayIndex]
                                                [unavailabilityStartIndex + i] = 1;
                            }
                        }
                    }
                }
            }
        }
        roomIndex++;
    }
    return roomAvailability;
}