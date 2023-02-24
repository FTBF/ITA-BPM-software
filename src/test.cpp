#include "UIO.h"
#include "LTC2333.h"

#include <zmq.h>
//x#include "yaml-cpp/yaml.h"
#include <iostream>
#include <cstring>

#include <vector>
#include <algorithm>
#include <chrono>
#include <memory>


int main()
{
    const int READS_PER_TRIGGER = 512 + 1;
    const int NUM_TRIGGERS = 1000;
    uint32_t chipMask = 0xff;

    uint32_t nChan = 0;
    for(int i = 0; i < 8; ++i)
    {
        if(chipMask & (1 << i)) nChan += 1;
    }

    //uint32_t buff[READS_PER_TRIGGER*8];
    std::unique_ptr<std::vector<uint32_t>> buff(new std::vector<uint32_t>(READS_PER_TRIGGER*NUM_TRIGGERS*(nChan+2)));

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

        ltc.read(buff->data()+iTrig*READS_PER_TRIGGER*(nChan+2)); //nChan + 2 to account for 64 bit timestamp
    }
    auto stop = std::chrono::system_clock::now();

    for(int i = 0; i < 9; ++i)
    {
        std::cout << "FIFOOcc: " << i << "\t" << ltc.getFIFOOcc(i) << std::endl;
    }

    std::cout << buff->size() << std::endl;
    for(int i = 1; i < READS_PER_TRIGGER; ++i)
    {
        printf("%6d:  %12llu  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d\n",  i,
               *(uint64_t*)(buff->data() + 2*i + 8*READS_PER_TRIGGER),
               0x7&((*buff)[i + 0*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 1*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 2*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 3*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 4*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 5*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 6*READS_PER_TRIGGER] >> 3),
               0x7&((*buff)[i + 7*READS_PER_TRIGGER] >> 3));
    }
    ltc.enableRead(false);

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
 
    std::cout << "run time: " << duration.count() << " us"  << std::endl;
    std::cout << "Data rate: " << double((READS_PER_TRIGGER-1)*NUM_TRIGGERS) / (double(duration.count())/1000000.0) / 1000.0 << " kHz"  << std::endl;
}
