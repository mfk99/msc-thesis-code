#include <iostream>
#include "encoding/semantics/scheduling_semantics.h"
#include "utils/input_parser/input_parser.h"
#include "encoding/scheduling_encoding_generator/scheduling_encoding_generator.h"
#include "utils/config/config.h"
using namespace std;

int main(int argc, char **argv)
{
    parseInput(argc, argv);
    if (opts.initialize)
        generateConfig(opts.generationVariables);

    if (opts.execute)
    {
        cout << "Executing... \n";
        runBenchMark();
    }

    return 0;
}
