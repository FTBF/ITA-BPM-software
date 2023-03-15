#ifndef LTC2333_H
#define LTC2333_H

#include "UIO.h"
#include "UDMABuf.h"
#include "DMACtrl.h"

#include <vector>

class LTC2333
{
public:

    LTC2333();

    void configure(const uint32_t& chipMask, const uint32_t& chanMask, const uint32_t& range = 0, const uint32_t& mode = 0, const uint32_t& nEvents = 512+1); 

    void setReadDepth(const uint32_t& depth);

    void setReadPeriod(const uint32_t& period);

    bool fifoReady(const uint32_t& chan, const uint32_t& NR = 1);

    bool writeInProgress();

    bool triggerPending();

    uint32_t getFIFOOcc(const uint32_t& chan);

    void trigger();

    void enableRead(bool enable);

    void reset();

    bool wait(const uint32_t& timeout = 1000);

    void IRQReset();
    
    void read(uint32_t* buff);

private:
    static constexpr char WRITE_NAME[]  = "LTC2333-write-axi-to-ipif-mux-1";
    static constexpr char READ_NAME[]   = "LTC2333-read-axi-to-ipif-mux-0";
    static constexpr char INTR_NAME[]   = "LTC2333-read-axi-intc-0";

    UDMABuf dma_buf_;
    DMACtrl dma_ctrl_;
    
    UIO read_;
    UIO write_;
    UIO interrupt_;

    uint32_t nEvents_, chipMask_;
};

#endif
