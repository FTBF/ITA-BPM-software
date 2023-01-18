#include "LTC2333.h"
#include <unistd.h>

LTC2333::LTC2333() : read_(READ_NAME), write_(WRITE_NAME)
{
    for(int i = 0; i < 8; ++i)
    {
	fifos_.emplace_back(FIFO_NAME_A+std::to_string(i)+FIFO_NAME_B);
    }
}

void LTC2333::configure(const uint32_t& chanMask, const uint32_t& range, const uint32_t& mode)
{
    write_[1] = (chanMask & 0xff) | ((mode & 0x1) << 8) | ((range & 0x7) << 16);
    write_[0] = 0x1;
}

void LTC2333::setReadDepth(const uint32_t& depth)
{
    write_[3] = depth;
}

void LTC2333::setReadPeriod(const uint32_t& period)
{
    write_[2] = period;
}

void LTC2333::trigger()
{
    write_[0] = 1;
}

void LTC2333::enableRead(bool enable)
{
    if(enable) read_[0] = 0x2;
    else       read_[0] = 0x0;
}

void LTC2333::reset()
{
    read_[0] = read_[0] | 0x1;
}

std::vector<uint32_t> LTC2333::read(const uint32_t& chip, const uint32_t& depth, const uint32_t& timeout)
{
    uint32_t time = 0;
    while(read_[1 + chip*4] < depth)
    {
	++time;
	if(time >= timeout) return std::vector<uint32_t>();
	else                usleep(100);
    }

    std::vector<uint32_t> data(depth);
    for(uint32_t i = 0; i < depth; ++i)
    {
	data[i] = fifos_[chip][0];
    }

    return data;
}
