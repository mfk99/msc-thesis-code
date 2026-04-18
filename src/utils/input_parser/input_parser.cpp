#include <iostream>
#include <vector>
#include "input_parser.h"
#include <cxxopts.hpp>
using namespace std;

Options opts;

void printHelp()
{
    cout << "Instructions:\n";
    cout << "-----------------\n";
    cout << "-h Show this help message\n";
    cout << "-v Enable verbose output (default: off)\n";
    cout << "-m Enable manual literal insertion (default: off)\n";
    cout << "-e Execute XML file (default: off)\n";
    cout << "-t Determines test type [1|2|3] (default: 1)\n";
    cout << "-I Generate XML file (initialization) (default: off)\n";
    cout << "-g Generate XML file for each encoding step (default: off)\n";
    cout << "-i <int> Number of iterations (default: 10)\n";
    cout << "-f <path> Input file path (default: empty)\n";
    cout << "-p <6 ints> Initialization parameters (e.g. 1,2,3,4,5,6)\n";
}

void parseInput(int argc, char **argv)
{
    cxxopts::Options options("msc-code", "Incremental SAT Benchmarking tool");
    options.add_options()                                                                         //
        ("h,help", "Display help")                                                                //
        ("v,verbose", "Verbose output")                                                           //
        ("m,manual_input", "Manual literal insertion")                                            //
        ("e,execute", "Execute xml file")                                                         //
        ("t,testType", "Determines test type [1|2|3]", cxxopts::value<int>()->default_value("1")) //
        ("I,initialize", "Generate xml file")                                                     //
        ("g,generate", "Generate xml file for each encoding step")                                //
        ("i,iterations", "Iteration amount", cxxopts::value<int>()->default_value("10"))          //
        ("f,file", "File path", cxxopts::value<string>()->default_value(""))                      //
        ("p,initParams", "Initialization parameters", cxxopts::value<vector<int>>());
    auto result = options.parse(argc, argv);

    if (result["help"].as<bool>())
    {
        printHelp();
        return;
    }

    opts.verbose = result.count("verbose") > 0;
    opts.manual_input = result.count("manual_input") > 0;
    opts.execute = result.count("execute") > 0;
    opts.initialize = result.count("initialize") > 0;
    opts.generate = result.count("generate") > 0;
    opts.iterations = result["iterations"].as<int>();
    opts.testType = result["testType"].as<int>();
    opts.filePath = result["file"].as<string>();

    if (result.count("initParams") > 0)
        opts.generationVariables = result["initParams"].as<vector<int>>();
    else
        opts.generationVariables = {};
}