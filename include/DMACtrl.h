#ifndef DMACTRL_H
#define DMACTRL_H

#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "UDMABuf.h"

class DMACtrl
{
public:
    UIO dma_;
    uint32_t count_, dmaSize_;
    UDMABuf sgBuf_;
    uint32_t nSGBuf_;
    uint32_t lastTailPtr_;

    DMACtrl(const std::string& dmaCtrl_name, const std::string& udmabuf_name) : dma_(dmaCtrl_name), sgBuf_(udmabuf_name, false), nSGBuf_(0), lastTailPtr_(0)
    {
    }

    void configureSGBuf(const UDMABuf& buf, const uint32_t length, const uint8_t chipMask)
    {
        //Addresses hardcoded, can this be avoided?
        const uint32_t sas[] = {
            0x76000000,
            0x76010000,
            0x76020000,
            0x76030000,
            0x76040000,
            0x76050000,
            0x76060000,
            0x76070000,
        };
        nSGBuf_ = 0;
        for(unsigned int i = 0; i < sgBuf_.size()/4; ++i) sgBuf_[i] = 0;
        for(unsigned int i = 0; i < 8; ++i)
        {
            if((1 << i) & chipMask)
            {
                sgBuf_[0 + nSGBuf_*128/4] = sgBuf_.physAddr() + (nSGBuf_ + 1)*128;
                sgBuf_[2 + nSGBuf_*128/4] = sas[i];
                sgBuf_[4 + nSGBuf_*128/4] = buf.physAddr() + length*nSGBuf_;
                sgBuf_[6 + nSGBuf_*128/4] = length;
                ++nSGBuf_;
            }
        }
        //close pointer ring
        sgBuf_[0 + (nSGBuf_-1)*128/4] = sgBuf_.physAddr();
        curdescPtr(sgBuf_.physAddr());
        lastTailPtr_ = sgBuf_.physAddr() + (nSGBuf_ - 1)*128;

        //set interrupt coalessing
        dma_[0] = (0xff00ffff & dma_[0]) | (nSGBuf_ << 16);
    }
    
    inline uint32_t nSGBuf()
    {
        return nSGBuf_;
    }

    inline bool sgBufReady(uint32_t bufNum)
    {
        return sgBuf_[bufNum*32 + 7] & 0x80000000;
    }

    inline bool sgBufError(uint32_t bufNum)
    {
        return sgBuf_[bufNum*32 + 7] & 0x70000000;
    }

    inline void sgBufReset(uint32_t bufNum)
    {
        sgBuf_[bufNum*32 + 7] = 0;
    }

    inline void curdescPtr(uint32_t ptr)
    {
        dma_[2] = ptr;
    }

    inline void taildescPtr(uint32_t ptr)
    {
        dma_[4] = ptr;
    }

    inline void startSG()
    {
        dma_[4] = lastTailPtr_;
    }

    inline bool isIdle()
    {
        return dma_[1] & 0x2;
    }

    inline void enableSG(bool enable)
    {
        // must not change scatter gather state while CDMA is not idle
        while(!isIdle()) usleep(100);

        dma_[0] = enable?(dma_[0]|(1<<3)):(dma_[0]&(~(1<<3)));
    }

    inline void reset()
    {
        dma_[0] = 1 << 2;
    }

    inline void keyholeWrite(bool enable)
    {
        dma_[0] = enable?(dma_[0]|(1<<5)):(dma_[0]&(~(1<<5)));
    }

    inline void keyholeRead(bool enable)
    {
        dma_[0] = enable?(dma_[0]|(1<<4)):(dma_[0]&(~(1<<4)));
    }

    inline void IRQEnable(bool enable)
    {
        dma_[0] = enable?(dma_[0]|(1<<12)):(dma_[0]&(~(1<<12)));
    }

    inline void IRQWait()
    {
        //while((dma_[1]&0x2) == 0);
        
        dma_.IRQWait();

        IRQReset();
    }

    inline void IRQReset()
    {
        dma_[1] = 1 << 12;
        dma_.IRQReset();
    }

    inline void transfer(uint32_t sa, uint32_t da, uint32_t size)
    {
        dma_[0x18/4] = sa;
        dma_[0x20/4] = da;
        dma_[0x28/4] = size;
    }

    ~DMACtrl()
    {
    }
};

#endif
