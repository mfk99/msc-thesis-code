#include <iostream>
#include <vector>
#include "input-parser.h"
using namespace std;

unsigned short verbose = 0;
unsigned short initialize = 0;
unsigned short generate = 0;
unsigned short execute = 0;

string filePath = "";

void parseInput(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i)
    {
        std::string str(argv[i]);
        if (str == "-v")
        {
            cout << "Verbose mode enabled \n";
            verbose = 1;
        }
        if (str == "-i")
        {
            cout << "Config initialization enabled \n";
            initialize = 1;
        }
        if (str == "-g")
        {
            cout << "Encoding generation enabled \n";
            generate = 1;
        }
        if (str == "-e")
        {
            cout << "Execution enabled \n";
            execute = 1;
        }
        if (str == "-n")
        {
            filePath = argv[i + 1];
        }
        if (str == "-help")
        {
            printHelp();
            return;
        }
    }
}

void printHelp()
{
    cout << "Instructions: \n";
    cout << "----------------- \n";
    cout << "-v Enable verbose logging, disabled by default \n";
    cout << "-i Enable config initialization, disabled by default \n";
    cout << "-g Enable encoding generation, disabled by default \n";
    cout << "-e Enable encoding execution, disabled by default \n";
    cout << "----------------- \n";
}

vector<int> parseEncodingGenerationVariables(int argc, char **argv)
{
    vector<int> userVariables;
    /*
    Arguments are give in the following order:
    1. days
    2. hours
    3. classRooms
    4. courses
    5. courseHours
    */
    int i = 1 + verbose + initialize + generate + execute;
    int i2 = i + 5;
    for (; i < i2; i++)
    {
        userVariables.push_back(atoi(argv[i]));
    }
    return userVariables;
}