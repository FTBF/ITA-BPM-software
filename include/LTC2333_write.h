#ifndef LTC2333_WRITE_H
#define LTC2333_WRITE_H

#include "UIO.h"

class LTC2333_write
{
public:

    LTC2333_write();

    void configure(const uint32_t& chanMask, const uint32_t& range = 0, const uint32_t& mode = 0); 

    void setReadDepth(const uint32_t& depth);

    void setReadPeriod(const uint32_t& period);

    void trigger();

private:
    static constexpr char WRITE_NAME[] = "LTC2333-write-axi-to-ipif-mux-1";
    
    UIO write;
};

#endif
