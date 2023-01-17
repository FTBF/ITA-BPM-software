#include "LTC2333_read.h"
#include <unistd.h>
#include <string>

LTC2333_read::LTC2333_read(const int& chip) : read_(READ_NAME), fifo_(FIFO_NAME_A+std::to_string(chip)+FIFO_NAME_B), iChip_(chip)
{
    
}

void LTC2333_read::configure(bool enable)
{
    if(enable) read_[0] = 0x2;
    else       read_[0] = 0x0;
}

void LTC2333_read::reset()
{
    read_[0] = read_[0] | 0x1;
}

std::vector<uint32_t> LTC2333_read::read(const uint32_t& depth, const uint32_t& timeout)
{
    uint32_t time = 0;
    while(read_[1 + iChip_*4] < depth)
    {
	++time;
	if(time >= timeout) return std::vector<uint32_t>();
	else                usleep(100);
    }

    std::vector<uint32_t> data(depth);
    for(int i = 0; i < depth; ++i)
    {
	data[i] = fifo_[0];
    }

    return data;
}
