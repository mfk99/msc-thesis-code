#include <iostream>
#include "settings.h"
using namespace std;

int debug = 0;

void enterSettings()
{
    while (true)
    {
        int x;
        cout << "Please insert command: \n (1) Toggle debug\n (0) Exit settings\n";
        cin >> x; // Get user input from the keyboard
        cout << "Your number is: " << x << "\n";
        if (x == 0)
        {
            break;
        }
        else if (x == 1)
        {
            toggleDebug();
        }
        else
        {
            cout << "Invalid option. Please try again.\n";
        }
    }
}

void toggleDebug()
{
    cout << "Toggling debug. It is currently " << debug << "\n";
    if (debug == 0)
        debug = 1;
    else
        debug = 0;
    cout << "Debug is now " << debug << "\n";
}