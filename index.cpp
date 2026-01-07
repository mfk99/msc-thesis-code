#include <iostream>
#include "semantics/scheduling-semantics.h"
#include "input-parser/input-parser.h"
using namespace std;

int main(int argc, char **argv)
{
    parseInput(argc, argv);

    if (generate)
    {
        cout << "Generating... \n";
        vector<int> generationParameters = parseEncodingGenerationVariables(argc, argv);
        enterIpamirSchedulingGenerator(generationParameters);
    }

    if (execute)
    {
        cout << "Executing... \n";
    }

    return 0;
}
