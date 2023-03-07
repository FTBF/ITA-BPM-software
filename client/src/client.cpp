#include <zmq.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "yaml-cpp/yaml.h"
#include <signal.h>
#include <getopt.h>

#include "DAQCtrl.h"

bool g_stop = false;

void signal_callback_handler(int signum) 
{
    std::cout << "Caught signal " << signum << std::endl;
    // Terminate program
    g_stop = true;
}

static void s_catch_signals (void)
{
    struct sigaction action;
    action.sa_handler = signal_callback_handler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);
}


int main (int argc, char *argv[]) 
{
    std::string configFile = "config.yaml";

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"cfg",     required_argument, 0,  'c' },
            {0,         0,                 0,  0 }
        };

        int c = getopt_long(argc, argv, "c:", long_options, &option_index);
        if (c == -1)
            break;

        switch (option_index) 
        {
        case 0:
            configFile = optarg;
            break;

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    YAML::Node config = YAML::LoadFile(configFile);
    zmq::context_t context (2);

    // Register signal and signal handler
    s_catch_signals();

    DAQCtrl daq(context, config);

    daq.setGStop(&g_stop);
    daq.sendConfig();
    daq.setBias();
    daq.readTemp(1);
    daq.readTemp(2);
    daq.startRun();

    return 0;
}
