#include <iostream>
#include "semantics/scheduling_semantics.h"
#include "input_parser/input_parser.h"
#include "scheduling_encoding_generator/scheduling_encoding_generator.h"
#include "config/config.h"
using namespace std;

int main(int argc, char **argv)
{

    parseInput(argc, argv);

    if (initialize)
    {
        vector<int> configVariables = parseEncodingGenerationVariables(argc, argv);
        generateConfig(configVariables);
    }

    if (execute)
    {
        cout << "Executing... \n";
        runBenchMark();
    }

    return 0;
}
