/***************************************************************
                            main.cpp
                               
UNFLoader Entrypoint
***************************************************************/ 

#include "main.h"
#include "helper.h"
#include "term.h"
#include "device.h"
#include <stdlib.h>
#include <string.h>
#include <list>
#include <iterator>

/*********************************
              Macros
*********************************/

#define PROGRAM_NAME_LONG  "Universal N64 Flashcart Loader"
#define PROGRAM_NAME_SHORT "UNFLoader"
#define PROGRAM_GITHUB     "https://github.com/buu342/N64-UNFLoader"


/*********************************
        Function Prototypes
*********************************/

void parse_args_priority(std::list<char*>* args);
void parse_args(std::list<char*>* args);
void show_title();
void show_args();
void show_help();
#define nextarg_isvalid(a, b) (((++a) != b->end()) && (*a)[0] != '-')


/*********************************
             Globals
*********************************/

// Program globals
FILE* global_debugoutptr = NULL;
std::atomic<progState> global_progstate (Initializing);
std::atomic<bool> global_escpressed (false);

// Local globals
static bool  local_debugmode = false;
static bool  local_listenmode = false;
static char* local_debugoutfilepath = NULL; // MOVE THIS OVER TO debug.cpp LATER
static char* local_binaryoutfolderpath = NULL; // MOVE THIS OVER TO debug.cpp LATER
static std::list<char*> local_args;


/*==============================
    main
    Program entrypoint
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

#include <thread>
#include <chrono>
int main(int argc, char* argv[])
{
    // Put the arguments in a list to make life easier
    for (int i=1; i<argc; i++)
        local_args.push_back(argv[i]);

    // Parse priority arguments
    parse_args_priority(&local_args);

    // Initialize the program
    term_initialize();
    show_title();

    // Read program arguments
    parse_args(&local_args);

    // Show the program arguments if the program can't do much else
    if (!local_debugmode && !local_listenmode && device_getrom() == NULL)
    {
        #ifndef LINUX
            int w = term_getw() < 40 ? 40 : term_getw();
            int h = term_geth() < 80 ? 80 : term_geth();
            term_setsize(w , h)
        #endif
        show_args();
        term_hideinput(true);
        terminate(NULL);
    }

    // Loop forever
    do 
    {
        ///*
        static int i = 0;   
        log_colored("Hello %d\n", CRDEF_PRINT, i++);
        //*/
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    while ((local_debugmode || local_listenmode) && !global_escpressed);

    // End the program
    terminate(NULL);
    return 0;
}


/*==============================
    parse_args_priority
    Parses priority arguments
    @param A list of arguments
==============================*/

void parse_args_priority(std::list<char*>* args)
{
    std::list<char*>::iterator it;

    // Check if curses should be disabled
    for (it = args->begin(); it != args->end(); ++it)
    {
        char* command = (*it);
        if (!strcmp(command, "-b"))
        {
            term_usecurses(false);
            break;
        }
    }

    // Check for the forced terminal size
    if (term_isusingcurses())
    {
        for (it = args->begin(); it != args->end(); ++it)
        {
            char* command = (*it);
            if (!strcmp(command, "-w"))
            {
                if (nextarg_isvalid(it, args))
                {
                    int h = atoi((*it));
                    if (nextarg_isvalid(it, args))
                    {
                        int w = atoi((*it));
                        term_initsize(h, w);
                        it = args->erase(it);
                        --it;
                        it = args->erase(it);
                        --it;
                        args->erase(it);
                    }
                    else
                        terminate("Missing parameter(s) for command '%s'.", command);
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            }
        }
}

    // Check if the help argument was requested
    for (it = args->begin(); it != args->end(); ++it)
    {
        char* command = (*it);
        if (!strcmp(command, "-help"))
        {
            term_initialize();
            show_title();
            term_hideinput(true);
            show_help();
            terminate(NULL);
        }
    }
}


/*==============================
    parse_args
    Parses the arguments passed to the program
    @param The number of extra arguments
    @param An array with the arguments
==============================*/

void parse_args(std::list<char*>* args)
{
    std::list<char*>::iterator it;

    // If the list is empty, nothing left to parse
    if (args->empty())
        return;

    // If the first character is not a dash, assume a ROM path
    if (args->front()[0] != '-')
    {
        device_setrom(args->front());
        return;
    }

    // Handle the rest of the program arguments
    for (it = args->begin(); it != args->end(); ++it)
    {
        char* command = (*it);

        // Only allow valid command formats
        if (command[0] != '-' || command[1] == '\0')
            terminate("Unknown command '%s'", command);

        // Handle the rest of the commands
        switch(command[1])
        {
            case 'r': // ROM to upload
                if (nextarg_isvalid(it, args))
                    device_setrom(*it);
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'f': // Set flashcart
                if (nextarg_isvalid(it, args))
                {
                    CartType cart = cart_strtotype(*it);
                    device_setcart(cart);
                    log_simple("Flashcart forced to '%s'\n", cart_typetostr(cart));
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'c': // Set CIC
                if (nextarg_isvalid(it, args))
                {
                    CICType cic = cic_strtotype(*it);
                    device_setcic(cic);
                    log_simple("CIC forced to '%s'\n", cic_typetostr(cic));
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 's': // Set save type
                if (nextarg_isvalid(it, args))
                {
                    SaveType save = save_strtotype(*it);
                    device_setsave(save);
                    log_simple("Save type set to '%s'\n", save_typetostr(save));
                }
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            case 'd': // Set debug mode
                local_debugmode = true;
                if (nextarg_isvalid(it, args))
                {
                    local_debugoutfilepath = *it;
                    log_simple("Debug logging to file '%s'", local_debugoutfilepath);
                }
                else
                    --it;
                break;
            case 'l': // Set listen mode
                local_listenmode = true;
                break;
            case 'e': // File export directory
                local_binaryoutfolderpath = *it;
                log_simple("File export path set to '%s'", local_binaryoutfolderpath);
                break;
            case 'h': // Set history size
                if (nextarg_isvalid(it, args))
                    term_sethistorysize(atoi(command));
                else
                    terminate("Missing parameter(s) for command '%s'.", command);
                break;
            default:
                terminate("Unknown command '%s'", command);
                break;
        }
    }
}


/*==============================
    show_title
    Prints the title of the program
==============================*/

void show_title()
{
    int i;
    char title[] = PROGRAM_NAME_SHORT;
    int titlesize = sizeof(title)/sizeof(title[0]);
 
    // Print the title
    for (i=0; i<titlesize-1; i++)
    {
        char str[2];
        str[0] = title[i];
        str[1] = '\0';
        log_colored(str, 1+(i)%(TOTAL_COLORS-1));
    }

    // Print info stuff
    log_simple("\n--------------------------------------------\n");
    log_simple("Cobbled together by Buu342\n");
    log_simple("Compiled on %s\n\n", __DATE__);
}


/*==============================
    show_args
    Prints the arguments of the program
==============================*/

void show_args()
{
    log_simple("Parameters: <required> [optional]\n");
    log_simple("  -help\t\t\t   Learn how to use this tool.\n");                                                                        // Done
    log_simple("  -r <file>\t\t   Upload ROM.\n");                                                                                      // Done
    log_simple("  -a\t\t\t   Disable ED ROM header autodetection.\n");
    log_simple("  -f <int>\t\t   Force flashcart type (skips autodetection).\n");                                                       // Done
    log_simple("  \t %d - %s\n", (int)CART_64DRIVE1, "64Drive HW1");                                                                    //
    log_simple("  \t %d - %s\n", (int)CART_64DRIVE2, "64Drive HW2");                                                                    //
    log_simple("  \t %d - %s\n", (int)CART_EVERDRIVE, "EverDrive 3.0 or X7");                                                           //
    log_simple("  \t %d - %s\n", (int)CART_SC64, "SC64");                                                                               //
    log_simple("  -c <int>\t\t   Set CIC emulation (64Drive HW2 only).\n");                                                             // Done
    log_simple("  \t %d - %s\t %d - %s\n", (int)CIC_6101, "6101 (NTSC)", (int)CIC_6102, "6102 (NTSC)");                                 //
    log_simple("  \t %d - %s\t %d - %s\n", (int)CIC_7101, "7101 (NTSC)", (int)CIC_7102, "7102 (PAL)");                                  //
    log_simple("  \t %d - %s\t\t %d - %s\n", (int)CIC_X103, "x103 (All)", (int)CIC_X105, "x105 (All)");                                 //
    log_simple("  \t %d - %s\t\t %d - %s\n", (int)CIC_X106, "x106 (All)", (int)CIC_5101, "5101 (NTSC)");                                //
    log_simple("  -s <int>\t\t   Set save emulation.\n");                                                                               // Done
    log_simple("  \t %d - %s\t %d - %s\n", (int)SAVE_EEPROM4K, "EEPROM 4Kbit", (int)SAVE_EEPROM16K, "EEPROM 16Kbit");                   //
    log_simple("  \t %d - %s\t %d - %s\n", (int)SAVE_SRAM256, "SRAM 256Kbit", (int)SAVE_FLASHRAM, "FlashRAM 1Mbit");                    //
    log_simple("  \t %d - %s\t %d - %s\n", (int)SAVE_SRAM768, "SRAM 768Kbit", (int)SAVE_FLASHRAMPKMN, "FlashRAM 1Mbit (PokeStdm2)");    //
    log_simple("  -d [filename]\t\t   Debug mode. Optionally write output to a file.\n");                                               // Done
    log_simple("  -l\t\t\t   Listen mode (reupload ROM when changed).\n");                                                              // Done
    log_simple("  -t <seconds>\t\t   Enable timeout (disables key press checks).\n");
    log_simple("  -e <directory>\t   File export directory (Folder must exist!).\n");                                                   // Done
    log_simple(            "\t\t\t   Example:  'folder/path/' or 'c:/folder/path'.\n");                                                 //
    log_simple("  -w <int> <int>\t   Force terminal size (number rows + columns).\n");                                                  // Not quite working?
    log_simple("  -h <int>\t\t   Max window history (default %d).\n", DEFAULT_HISTORYSIZE);                                             // Done
    log_simple("  -m \t\t\t   Always show duplicate prints in debug mode.\n");
    log_simple("  -b\t\t\t   Disable ncurses.\n");                                                                                      // Done
}


/*==============================
    show_help
    Prints the help information
==============================*/

void show_help()
{
    int category = 0;

    // Print the introductory message
    log_simple("Welcome to the %s!\n", PROGRAM_NAME_LONG);
    log_simple("This tool is designed to upload ROMs to your N64 Flashcart via USB, to allow\n"
               "homebrew developers to debug their ROMs in realtime with the help of the\n"
               "library provided with this tool.\n\n");
    log_simple("Which category are you interested in?\n"
               " 1 - Uploading ROMs on the 64Drive\n"
               " 2 - Uploading ROMs on the EverDrive\n"
               " 3 - Uploading ROMs on the SC64\n"
               " 4 - Using Listen mode\n"
               " 5 - Using Debug mode\n");

    // Get the category
    log_colored("\nCategory: ", CRDEF_INPUT);
    category = getchar();
    if (term_isusingcurses())
        log_colored("%c\n\n", CRDEF_INPUT, category);

    // Print the category contents
    switch (category)
    {
        case '1':
            log_simple(" 1) Ensure your device is on the latest firmware/version.\n"
                       " 2) Plug your 64Drive USB into your PC, ensuring the console is turned OFF.\n"
                       " 3) Run this program to upload a ROM. Example:\n" 
                       " \t unfloader.exe -r myrom.n64\n");
            log_simple(" 4) If using 64Drive HW2, your game might not boot if you do not state the\n"
                       "    correct CIC as an argument. Most likely, you are using CIC 6102, so simply\n"
                       "    append that to the end of the arguments. Example:\n"
                       " \t unfloader.exe -r myrom.n64 -c 6102\n"
                       " 5) Once the upload process is finished, turn the console on. Your ROM should\n"
                       "    execute.\n");
            break;
        case '2':
            log_simple(" 1) Ensure your device is on the latest firmware/version for your cart model.\n"
                        " 2) Plug your EverDrive USB into your PC, ensuring the console is turned ON and\n"
                        "    in the main menu.\n"
                        " 3) Run this program to upload a ROM. Example:\n" 
                        " \t unfloader.exe -r myrom.n64\n"
                        " 4) Once the upload process is finished, your ROM should execute.\n");
            break;
        case '3':
            log_simple(" 1) Plug the SC64 USB into your PC.\n"
                       " 2) Run this program to upload a ROM. Example:\n" 
                       " \t unfloader.exe -r myrom.n64\n"
                       " 3) Once the upload process is finished, your ROM should execute.\n");
            break;
        case '4':
            log_simple("Listen mode automatically re-uploads the ROM via USB when it is modified. This\n"
                    "saves you the trouble of having to restart this program every recompile of your\n"
                    "homebrew. It is on YOU to ensure the cart is prepared to receive another ROM.\n"
                    "That means that the console must be switched OFF if you're using the 64Drive or\n"
                    "be in the menu if you're using an EverDrive. In SC64 case ROM can be uploaded\n"
                    "while console is running but if currently running code is actively accessing\n"
                    "ROM space this can result in glitches or even crash, proceed with caution.\n");
            break;
        case '5':
            log_simple("In order to use the debug mode, the N64 ROM that you are executing must already\n"
                       "have implented the USB or debug library that comes with this tool. Otherwise,\n"
                       "debug mode will serve no purpose.\n\n");
            log_simple("During debug mode, you are able to type commands, which show up in ");
            log_colored(                                                                   "green", CRDEF_INPUT);
            log_simple(                                                                          " on\n"
                       "the bottom of the terminal. You can press ENTER to send this command to the N64\n"
                       "as the ROM executes. The command you send must obviously be implemented by the\n"
                       "debug library, and can do various things, such as upload binary files, take\n"
                       "screenshots, or change things in the game. If you wrap a part of your command\n"
                       "with the '@' symbol, the tool will treat that part as a file and will upload it\n"
                       "along with the rest of the data.\n\n");
            log_simple("During execution, the ROM is free to print things to the console where this\n"
                       "program is running. Messages from the console will appear in ");
            log_colored(                                                             "yellow", CRDEF_PRINT);
            log_simple(                                                                     ".\n\n"
                    "For more information on how to implement the debug library, check the GitHub\n"
                    "page where this tool was uploaded to, there should be plenty of examples there.\n"
                    PROGRAM_GITHUB"\n");
            break;
        default:
            terminate("Unknown category."); 
    }
    
    // Clear leftover input
    if (!term_isusingcurses())
        while ((category = getchar()) != '\n' && category != EOF);
}