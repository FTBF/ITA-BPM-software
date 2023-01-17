#ifndef LTC2333_READ_H
#define LTC2333_READ_H

#include "UIO.h"

#include <vector>

class LTC2333_read
{
public:

    LTC2333_read(const int& chip);

    void configure(bool enable);

    void reset();

    std::vector<uint32_t> read(const uint32_t& depth, const uint32_t& timeout = 1000);
    
private:
    static constexpr char READ_NAME[] = "LTC2333-read-axi-to-ipif-mux-0";
    static constexpr char FIFO_NAME_A[] = "LTC2333-read-LTC233-read-block-";
    static constexpr char FIFO_NAME_B[] = "-AXI-Full-IPIF-0";

    UIO read_, fifo_;
    int iChip_;
};

#endif
