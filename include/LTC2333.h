#ifndef LTC2333_H
#define LTC2333_H

#include "UIO.h"

#include <vector>

class LTC2333
{
public:

    LTC2333();

    void configure(const uint32_t& chanMask, const uint32_t& range = 0, const uint32_t& mode = 0); 

    void setReadDepth(const uint32_t& depth);

    void setReadPeriod(const uint32_t& period);

    void trigger();

    void enableRead(bool enable);

    void reset();

    std::vector<uint32_t> read(const uint32_t& chip, const uint32_t& depth, const uint32_t& timeout = 1000);

private:
    static constexpr char WRITE_NAME[]  = "LTC2333-write-axi-to-ipif-mux-1";
    static constexpr char READ_NAME[]   = "LTC2333-read-axi-to-ipif-mux-0";
    static constexpr char FIFO_NAME_A[] = "LTC2333-read-LTC233-read-block-";
    static constexpr char FIFO_NAME_B[] = "-AXI-Full-IPIF-0";

    UIO read_;
    UIO write_;
    std::vector<UIO> fifos_;
};

#endif
