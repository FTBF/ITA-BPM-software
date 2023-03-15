#include "LTC2333.h"
#include <unistd.h>

#include <iostream>

LTC2333::LTC2333() : dma_buf_("udmabuf0"), dma_ctrl_("AXI-and-resets-axi-cdma-0", "udmabuf1"), read_(READ_NAME), write_(WRITE_NAME), interrupt_(INTR_NAME), nEvents_(0), chipMask_(0)
{
    dma_buf_.syncOffset(0);
    dma_buf_.syncCpu();
    for(unsigned int i = 0; i < dma_buf_.size()/4; ++i) dma_buf_[i] = 0;

    dma_ctrl_.reset();
    usleep(1000);
    dma_ctrl_.enableSG(true);
    dma_ctrl_.keyholeRead(true);
    dma_ctrl_.IRQEnable(true);
    dma_ctrl_.IRQReset();
}

void LTC2333::configure(const uint32_t& chipMask, const uint32_t& chanMask, const uint32_t& range, const uint32_t& mode, const uint32_t& nEvents)
{
    nEvents_ = nEvents;
    chipMask_ = chipMask;
    
    dma_buf_.syncSize((nEvents)*10*sizeof(uint32_t));
    dma_ctrl_.configureSGBuf(dma_buf_, nEvents*sizeof(uint32_t), chipMask);

    write_[1] = (chanMask & 0xff) | ((mode & 0x1) << 8) | ((range & 0x7) << 16);

    interrupt_[2] = chipMask;
    interrupt_[7] = 0x3;
    read_[2] = nEvents;
}

void LTC2333::setReadDepth(const uint32_t& depth)
{
    write_[3] = depth;
}

void LTC2333::setReadPeriod(const uint32_t& period)
{
    write_[2] = period;
}

bool LTC2333::writeInProgress()
{
    return write_[0] & 0x2;
}

bool LTC2333::triggerPending()
{
    return write_[0] & 0x4;
}

void LTC2333::trigger()
{
    write_[0] = 1;
}

bool LTC2333::fifoReady(const uint32_t& chan, const uint32_t& NR)
{
    return read_[1+4*chan] >= NR;
}

uint32_t LTC2333::getFIFOOcc(const uint32_t& chan)
{
    return read_[1+4*chan];
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

bool LTC2333::wait(const uint32_t& timeout)
{
    for(int ichip = 0; ichip < 8; ++ichip)
    {
        if((1 << ichip)&chipMask_)
        {
            uint32_t time = 0;
            while(read_[1 + ichip*4] < nEvents_)
            {
            }
            break;
        }
    }

    //interrupt_.IRQWait();

    return true;
}

void LTC2333::IRQReset()
{
    interrupt_[3] = 0x1ff;
    interrupt_.IRQReset();
}

void LTC2333::read(uint32_t* buff)
{
    dma_buf_.syncDevice();
    dma_ctrl_.startSG();
    dma_ctrl_.IRQWait();
    dma_ctrl_.IRQReset();
    dma_buf_.syncCpu();

    memcpy(buff, dma_buf_.mem(), (dma_ctrl_.nSGBuf_+1)*nEvents_*sizeof(uint32_t)); // +1 because timestamp is 64 bits

    for(unsigned int i = 0; i < dma_ctrl_.nSGBuf_; ++i) dma_ctrl_.sgBufReset(i);
    //IRQReset();
}
