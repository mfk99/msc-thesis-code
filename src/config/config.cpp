#include <vector>
#include <map>
#include <string>
#include <cstring>
#include "../../libs/pugixml-1.15/src/pugixml.hpp"
#include <fstream>
#include "../input-parser/input-parser.h"
#include <iostream>
#include "DistributionTypes.h"
#include "ClassTypes.h"

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
                    for (int room = 1; room <= rooms; room++)
                    {
                        xml_node roomXmlNode = lectureXmlNode.append_child("room");
                        roomXmlNode.append_attribute("id") = room;
                        roomXmlNode.append_attribute("penalty") = 0;
                    }
                    xml_node timeXmlNode = lectureXmlNode.append_child("time");
                    timeXmlNode.append_attribute("days") = string(days, '1');
                    timeXmlNode.append_attribute("start") = 0;
                    timeXmlNode.append_attribute("length") = 1;
                    timeXmlNode.append_attribute("weeks") = string(weeks, '1');
                    timeXmlNode.append_attribute("penalty") = 0;
                }
            }
        }
        classIds.push_back(courseClassIds);
    }

    // TODO: add the rest
    // Add default SameStart constraint
    for (int course = 0; course < courses; course++)
    {
        xml_node precedenceXmlNode = distributionsXmlNode.append_child("distribution");
        precedenceXmlNode.append_attribute("type") = "SameStart";
        precedenceXmlNode.append_attribute("required") = "true";
        for (string classId : classIds[course])
        {
            xml_node classXmlNode = precedenceXmlNode.append_child("class");
            classXmlNode.append_attribute("id") = classId;
        }
    }

    // Add default SameDays constraint
    for (int course = 0; course < courses; course++)
    {
        xml_node precedenceXmlNode = distributionsXmlNode.append_child("distribution");
        precedenceXmlNode.append_attribute("type") = "SameDays";
        precedenceXmlNode.append_attribute("required") = "true";
        for (string classId : classIds[course])
        {
            xml_node classXmlNode = precedenceXmlNode.append_child("class");
            classXmlNode.append_attribute("id") = classId;
        }
    }

    // Add blank DifferentDays constraint
    xml_node differentDaysXmlNode = distributionsXmlNode.append_child("distribution");
    differentDaysXmlNode.append_attribute("type") = "DifferentDays";
    differentDaysXmlNode.append_attribute("required") = "false";
    differentDaysXmlNode.append_attribute("penalty") = "0";

    // Add default SameWeeks constraint
    for (int course = 0; course < courses; course++)
    {
        xml_node precedenceXmlNode = distributionsXmlNode.append_child("distribution");
        precedenceXmlNode.append_attribute("type") = "SameWeeks";
        precedenceXmlNode.append_attribute("required") = "true";
        for (string classId : classIds[course])
        {
            xml_node classXmlNode = precedenceXmlNode.append_child("class");
            classXmlNode.append_attribute("id") = classId;
        }
    }

    // Add blank DifferentWeeks constraint
    xml_node differentWeeksXmlNode = distributionsXmlNode.append_child("distribution");
    differentWeeksXmlNode.append_attribute("type") = "DifferentWeeks";
    differentWeeksXmlNode.append_attribute("required") = "false";
    differentWeeksXmlNode.append_attribute("penalty") = "0";

    // Add default SameRoom constraint
    for (int course = 0; course < courses; course++)
    {
        xml_node sameRoomXmlNode = distributionsXmlNode.append_child("distribution");
        sameRoomXmlNode.append_attribute("type") = "SameRoom";
        sameRoomXmlNode.append_attribute("required") = "true";
        for (string classId : classIds[course])
        {
            xml_node classXmlNode = sameRoomXmlNode.append_child("class");
            classXmlNode.append_attribute("id") = classId;
        }
    }

    // Add blank DifferentRooms constraint
    xml_node differentRoomXmlNode = distributionsXmlNode.append_child("distribution");
    differentRoomXmlNode.append_attribute("type") = "DifferentRoom";
    differentRoomXmlNode.append_attribute("required") = "false";
    differentRoomXmlNode.append_attribute("penalty") = "0";

    // Add Precedence constraint
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
    for ([[maybe_unused]] xml_node _ : roomsNode.children())
        rooms++;

    int courses = 0;
    xml_node coursesNode = problemNode.child("courses");
    for ([[maybe_unused]] xml_node _ : coursesNode.children())
        courses++;

    int courseHours = 0;
    for ([[maybe_unused]] xml_node _ : coursesNode.child("course").child("config").child("subpart").children())
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

map<string, Class> getClasses()
{
    map<string, Class> classes;

    char *filePathChar = new char[filePath.length() + 1];
    strcpy(filePathChar, filePath.c_str());
    xml_document doc;
    xml_parse_result result = doc.load_file(filePathChar);
    if (verbose)
        cout << "[VERBOSE] Load result: " << result.description() << "\n";

    xml_node coursesNode = doc.child("problem").child("courses");

    for (xml_node courseNode : coursesNode.children())
    {
        for (xml_node configNode : courseNode.children())
        {
            for (xml_node subpartNode : configNode.children())
            {
                for (xml_node classNode : subpartNode.children())
                {
                    Class newClass;
                    newClass.id = classNode.attribute("id").as_string();

                    vector<Room> newRoomVector;
                    for (xml_node roomNode : classNode.children("room"))
                    {
                        Room newRoom;
                        newRoom.id = roomNode.attribute("id").as_string();
                        newRoom.penalty = roomNode.attribute("penalty").as_int();

                        vector<RoomUnavailability> unavailabilityVec;
                        xml_node roomXmlNode = doc.child("problem").child("rooms").find_child_by_attribute("room", "id", newRoom.id.c_str());
                        for (xml_node unavailabilityXmlNode : roomXmlNode.children("unavailable"))
                        {
                            RoomUnavailability unavailability;
                            unavailability.days = unavailabilityXmlNode.attribute("days").as_string();
                            unavailability.weeks = unavailabilityXmlNode.attribute("weeks").as_string();
                            unavailability.start = unavailabilityXmlNode.attribute("start").as_int();
                            unavailability.length = unavailabilityXmlNode.attribute("length").as_int();
                            unavailabilityVec.push_back(unavailability);
                        }
                        newRoom.unavailability = unavailabilityVec;

                        map<string, int> travelTimeMap;
                        for (xml_node travelTimeXmlNode : roomXmlNode.children("travel"))
                        {
                            string travelDestinationId = travelTimeXmlNode.attribute("room").as_string();
                            int travelTime = travelTimeXmlNode.attribute("value").as_int();
                            travelTimeMap[travelDestinationId] = travelTime;
                        }
                        newRoom.travelTimes = travelTimeMap;

                        newRoomVector.push_back(newRoom);
                    }

                    vector<Timing> newTimingVector;
                    for (xml_node timeNode : classNode.children("time"))
                    {
                        Timing newTiming;
                        newTiming.days = timeNode.attribute("days").as_string();
                        newTiming.weeks = timeNode.attribute("weeks").as_string();
                        newTiming.start = timeNode.attribute("start").as_int();
                        newTiming.length = timeNode.attribute("length").as_int();
                        newTiming.penalty = timeNode.attribute("penalty").as_int();
                        newTimingVector.push_back(newTiming);
                    }

                    newClass.rooms = newRoomVector;
                    newClass.timings = newTimingVector;

                    string classId = classNode.attribute("id").as_string();

                    classes[classId] = newClass;
                }
            }
        }
    }
    return classes;
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

map<string, vector<Distribution>> getDistributions()
{
    map<string, vector<Distribution>> distributions;

    char *filePathChar = new char[filePath.length() + 1];
    strcpy(filePathChar, filePath.c_str());
    xml_document doc;
    xml_parse_result result = doc.load_file(filePathChar);
    if (verbose)
        cout << "[VERBOSE] Load result: " << result.description() << "\n";

    xml_node distributionsNode = doc.child("problem").child("distributions");

    for (xml_node distributionNode : distributionsNode.children())
    {

        string distributionType = distributionNode.attribute("type").as_string();

        bool required = 0;
        if (distributionNode.attribute("required"))
        {
            string requiredStr = distributionNode.attribute("required").as_string();
            required = requiredStr == "true";
        }

        int penalty = 0;
        if (distributionNode.attribute("penalty"))
        {
            penalty = distributionNode.attribute("penalty").as_int();
        }

        vector<string> classes;
        for (xml_node classNode : distributionNode.children())
        {
            string className = classNode.attribute("id").as_string();
            classes.push_back(className);
        }

        Distribution newDistribution;
        newDistribution.required = required;
        newDistribution.penalty = penalty;
        newDistribution.classes = classes;

        if (distributions.count(distributionType))
        {
            distributions[distributionType].push_back(newDistribution);
        }
        else
        {
            distributions[distributionType] = vector<Distribution>{newDistribution};
        }
    }

    return distributions;
}