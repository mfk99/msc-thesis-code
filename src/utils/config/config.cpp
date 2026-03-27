#include <vector>
#include <map>
#include <string>
#include <cstring>
#include "../../../libs/pugixml/src/pugixml.hpp"
#include <fstream>
#include "../input_parser/input_parser.h"
#include <iostream>
#include "DistributionTypes.h"
#include "ClassTypes.h"

using namespace std;
using namespace pugi;

/* The style of creating a configuration file
largely adheres to the ITC 2019 data format.
More info available at: https://www.itc2019.org/format
*/

xml_document openXmlFile()
{
    char *filePathChar = new char[opts.filePath.length() + 1];
    strcpy(filePathChar, opts.filePath.c_str());
    xml_document doc;
    xml_parse_result result = doc.load_file(filePathChar);

    if (!result)
    {
        cout << "[ERROR] Error when loading file: \n"
             << result.description()
             << " \nExiting... \n";
        exit(1);
    }
    else
    {
        if (opts.verbose)
            cout << "[VERBOSE] File loaded successfully \n";
        return doc;
    }
}

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
    root.append_attribute("name") = opts.filePath;
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

    char *filePathChar = new char[opts.filePath.length() + 1];
    strcpy(filePathChar, opts.filePath.c_str());
    doc.save_file(filePathChar);
}

vector<int> getConfigVariables()
{
    xml_document doc = openXmlFile();

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
    xml_document doc = openXmlFile();
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
    xml_document doc = openXmlFile();
    xml_node coursesNode = doc.child("problem").child("courses");

    for (xml_node courseNode : coursesNode.children())
    {
        for (xml_node configNode : courseNode.children())
        {
            // string configId = configNode.attribute("id").as_string();
            // cout << "configId: " << configId << "\n";
            for (xml_node subpartNode : configNode.children())
            {
                // string subPartId = subpartNode.attribute("id").as_string();
                // cout << "subpartNode: " << subPartId << "\n";
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

vector<HierarchyCourse> getClassHierarchy()
{
    xml_document doc = openXmlFile();
    xml_node problemNode = doc.child("problem");
    xml_node coursesNode = problemNode.child("courses");
    vector<HierarchyCourse> courses;
    for (xml_node courseNode : coursesNode.children())
    {
        HierarchyCourse newCourse;
        newCourse.id = courseNode.attribute("id").as_string();
        for (xml_node configNode : courseNode.children())
        {
            HierarchyConfiguration config;
            config.id = configNode.attribute("id").as_string();
            for (xml_node subPartNode : configNode.children())
            {
                HierarchySubpart subpart;
                subpart.id = subPartNode.attribute("id").as_string();
                for (xml_node classNode : subPartNode.children())
                {
                    HierarchyClass hierarchyClass;
                    hierarchyClass.id = classNode.attribute("id").as_string();
                    hierarchyClass.limit = classNode.attribute("limit").as_int();
                    if (classNode.attribute("parent"))
                        hierarchyClass.parentId = classNode.attribute("parent").as_string();
                    else
                        hierarchyClass.parentId = "";

                    subpart.classes.push_back(hierarchyClass);
                }
                config.subparts.push_back(subpart);
            }
            newCourse.configs.push_back(config);
        }
        courses.push_back(newCourse);
    }
    return courses;
}

vector<Student> getStudents()
{
    xml_document doc = openXmlFile();
    xml_node problemNode = doc.child("problem");
    xml_node studentsNode = problemNode.child("students");
    vector<Student> studentVec;
    for (xml_node studentNode : studentsNode.children())
    {
        Student newStudent;
        newStudent.id = studentNode.attribute("id").as_string();
        for (xml_node courseNode : studentNode.children())
        {
            newStudent.classIds.push_back(courseNode.attribute("id").as_string());
        }
        studentVec.push_back(newStudent);
    }
    return studentVec;
}

vector<vector<vector<vector<int>>>> getRoomAvailability()
{
    xml_document doc = openXmlFile();
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

void parseDistribution(xml_node distributionNode, map<string, vector<DistributionVariant>> &distributions)
{

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

    string distributionString = distributionNode.attribute("type").as_string();
    size_t openParen = distributionString.find('(');

    if (openParen == string::npos)
    {
        distributions[distributionString].emplace_back(Distribution(required, penalty, classes));
        return;
    }

    string distributionType = distributionString.substr(0, openParen);
    size_t closeParen = distributionString.find(')', openParen);
    if (closeParen == string::npos)
    {
        cout << "Closing parenthesis not found, exiting. \n";
        throw;
    }
    string parametersStr = distributionString.substr(openParen + 1, closeParen - openParen - 1);
    vector<int> parameters;
    size_t pos = 0;
    string parameterStr;

    while ((pos = parametersStr.find(',')) != string::npos)
    {
        parameterStr = parametersStr.substr(0, pos);
        parameters.push_back(stoi(parameterStr));
        parametersStr.erase(0, pos + 1);
    }

    if (!parametersStr.empty())
    {
        parameters.push_back(stoi(parametersStr));
    }

    if (parameters.size() > 2)
    {
        cout << "Expected maximum of 2 distribution parameters, found " << parameters.size() << ", exiting. \n";
        throw;
    }

    if (distributionType == "WorkDay")
    {
        distributions[distributionType].emplace_back(WorkDayDistribution(required, penalty, classes, parameters[0]));
        return;
    }
    else if (distributionType == "MinGap")
    {
        distributions[distributionType].emplace_back(MinGapDistribution(required, penalty, classes, parameters[0]));
        return;
    }
    else if (distributionType == "MaxDays")
    {
        distributions[distributionType].emplace_back(MaxDaysDistribution(required, penalty, classes, parameters[0]));
        return;
    }
    else if (distributionType == "MaxDayLoad")
    {
        distributions[distributionType].emplace_back(MaxDayLoadDistribution(required, penalty, classes, parameters[0]));
        return;
    }
    else if (distributionType == "MaxBreaks")
    {
        distributions[distributionType].emplace_back(MaxBreaksDistribution(required, penalty, classes, parameters[0], parameters[1]));
        return;
    }
    else if (distributionType == "MaxBlock")
    {
        distributions[distributionType].emplace_back(MaxBlockDistribution(required, penalty, classes, parameters[0], parameters[1]));
        return;
    }

    cout << "Couldn't match distribution type:" << distributionType << ", exiting. \n";
    throw;
}

map<string, vector<DistributionVariant>> getDistributions()
{
    map<string, vector<DistributionVariant>> distributions;

    xml_document doc = openXmlFile();
    xml_node distributionsNode = doc.child("problem").child("distributions");

    for (xml_node distributionNode : distributionsNode.children())
    {
        parseDistribution(distributionNode, distributions);
    }

    return distributions;
}