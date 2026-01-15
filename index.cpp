#include <iostream>
#include "semantics/scheduling-semantics.h"
#include "input-parser/input-parser.h"
#include "scheduling-encoding-generator/scheduling-encoding-generator.h"
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
