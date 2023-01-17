#include "LTC2333_write.h"

LTC2333_write::LTC2333_write() : write(WRITE_NAME)
{
    
}

void LTC2333_write::configure(const uint32_t& chanMask, const uint32_t& range, const uint32_t& mode)
{
    write[1] = (chanMask & 0xff) | ((mode & 0x1) << 8) | ((range & 0x7) << 16);
    write[0] = 0x1;
}

void LTC2333_write::setReadDepth(const uint32_t& depth)
{
    write[3] = depth;
}

void LTC2333_write::setReadPeriod(const uint32_t& period)
{
    write[2] = period;
}

void LTC2333_write::trigger()
{
    write[0] = 1;
}

