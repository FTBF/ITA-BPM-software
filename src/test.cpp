#include "UIO.h"
#include "LTC2333.h"

#include <zmq.h>
//x#include "yaml-cpp/yaml.h"
#include <iostream>
#include <cstring>

#include <vector>
#include <algorithm>
#include <chrono>


int main()
{
    const int READS_PER_TRIGGER = 512 + 1;
    const int NUM_TRIGGERS = 1000;
    uint32_t chipMask = 0x0f;

    //uint32_t buff[READS_PER_TRIGGER*8];
    std::vector<uint32_t>* buff = new std::vector<uint32_t>(READS_PER_TRIGGER*NUM_TRIGGERS*4);

    //LED control gpio
    UIO gpio("axi-gpio-0");
    gpio[0]=000;

    LTC2333 ltc;

    ltc.configure(chipMask, 0xff, 0);
    ltc.setReadDepth(READS_PER_TRIGGER);
    ltc.setReadPeriod(0);

    while(ltc.writeInProgress()) usleep(10);

    ltc.reset();

    ltc.enableRead(true);
    
    ltc.trigger();
    while(!ltc.fifoReady(0, READS_PER_TRIGGER)) usleep(10);

    auto start = std::chrono::system_clock::now();

    for(int iTrig = 0; iTrig < NUM_TRIGGERS; ++iTrig)
    {
        while(ltc.writeInProgress());
        ltc.trigger();

        ltc.wait();

        ltc.read(buff->data()+iTrig*READS_PER_TRIGGER*4);
    }

    auto stop = std::chrono::system_clock::now();

    for(int i = 0; i < 4; ++i)
    {
        std::cout << "FIFOOcc: " << i << "\t" << ltc.getFIFOOcc(i) << std::endl;
    }
    
    for(int i = 1; i < READS_PER_TRIGGER; ++i)
    {
        std::cout << i << "\t" << (0x7&((*buff)[i] >> 3))
                  << "\t" << (0x7&((*buff)[i + READS_PER_TRIGGER] >> 3))
                  << "\t" << (0x7&((*buff)[i + 2*(READS_PER_TRIGGER)] >> 3))
                  << "\t" << (0x7&((*buff)[i + 3*(READS_PER_TRIGGER)] >> 3)) << std::endl;
    }
    ltc.enableRead(false);

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
 
    std::cout << "run time: " << duration.count() << " us"  << std::endl;
    std::cout << "Data rate: " << double((READS_PER_TRIGGER-1)*NUM_TRIGGERS) / (double(duration.count())/1000000.0) / 1000.0 << " kHz"  << std::endl;
}
