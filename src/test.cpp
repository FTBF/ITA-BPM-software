#include "UIO.h"
#include "LTC2333.h"

#include <zmq.h>
#include "yaml-cpp/yaml.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <vector>
#include <algorithm>

const char attr_sync_[2] = "1";

class Buffer
{
    int fd_, fd_sync_device_, fd_sync_cpu_, fd_sync_offset_, fd_sync_size_;
    uint32_t *mem_;
    uint32_t physAddr_;
    uint32_t size_;
    const std::string name_;

public:

    Buffer(const std::string& name, bool cache = true) : name_(name)
    {
        if(cache) fd_ = open(("/dev/" + name).c_str(), O_RDWR);
        else      fd_ = open(("/dev/" + name).c_str(), O_RDWR | O_SYNC);

        char  attr[1024];
        unsigned long  sync_direction = 2;
        unsigned long  sync_offset = 0;
        int fd;

        if ((fd  = open(("/sys/class/u-dma-buf/" + name + "/size").c_str(), O_RDONLY)) != -1) 
        {
            char buffer[64];
            read(fd, buffer, 63);
            sscanf(buffer, "%u", &size_);
            close(fd);
        }
        else
        {
            printf("Unable to open size\n");
            exit(1);        
        }

        if (fd_ == -1) 
        {
            printf("Unable to open %s\n", ("/dev/" + name).c_str());
            exit(1);
        }
        
        mem_=(uint32_t*) (mmap(0,size_,PROT_READ|PROT_WRITE,MAP_SHARED, fd_, 0));
        if (mem_ == (uint32_t*)-1)
        {
            printf("Unable to memory map udmabuf\n");
            exit(1);
        }

        if ((fd  = open(("/sys/class/u-dma-buf/" + name + "/sync_direction").c_str(), O_WRONLY)) != -1) 
        {
            sprintf(attr, "%lu", sync_direction);
            write(fd, attr, strlen(attr));
            close(fd);
        }
        else
        {
            printf("Unable to open sync_direction\n");
            exit(1);        
        }
        
        if ((fd_sync_offset_ = open(("/sys/class/u-dma-buf/" + name + "/sync_offset").c_str(), O_WRONLY)) != -1) 
        {
            sprintf(attr, "%lu", sync_offset);
            write(fd_sync_offset_, attr, strlen(attr));
        }
        else
        {
            printf("Unable to open sync_offset\n");
            exit(1);        
        }
        
        if ((fd_sync_size_ = open(("/sys/class/u-dma-buf/" + name + "/sync_size").c_str(), O_WRONLY)) != -1) 
        {
            sprintf(attr, "%u", size_);
            write(fd_sync_size_, attr, strlen(attr));
        }
        else
        {
            printf("Unable to open sync_size\n");
            exit(1);        
        }
        
        if ((fd  = open(("/sys/class/u-dma-buf/" + name + "/phys_addr").c_str(), O_RDONLY)) != -1) 
        {
            char buffer[64];
            read(fd, buffer, 63);
            sscanf(buffer, "%x", &physAddr_);
            close(fd);
        }
        else
        {
            printf("Unable to open phys_addr\n");
            exit(1);        
        }

        fd_sync_device_ = open(("/sys/class/u-dma-buf/" + name + "/sync_for_device").c_str(), O_WRONLY);
        if(fd_sync_device_ == -1)
        {
            printf("Unable to open sync_for_device\n");
            exit(1);        
        }
        fd_sync_cpu_ = open(("/sys/class/u-dma-buf/" + name + "/sync_for_cpu").c_str(), O_WRONLY);
        if( fd_sync_cpu_ == -1)
        {
            printf("Unable to open sync_for_cpu\n");
            exit(1);        
        }
    }

    ~Buffer()
    {
        munmap(mem_, size_);
        
        close(fd_);
        close(fd_sync_cpu_);
        close(fd_sync_device_);
        close(fd_sync_offset_);
        close(fd_sync_size_);
    }

    inline uint32_t& operator[](const uint32_t& index)
    {
        return mem_[index];
    }
    
    inline uint32_t physAddr() const
    {
        return physAddr_;
    }
    
    inline uint32_t* mem() const
    {
        return mem_;
    }

    inline uint32_t size() const
    {
        return size_;
    }
    
    inline void syncCpu()
    {
        write(fd_sync_cpu_, attr_sync_, strlen(attr_sync_));
    }
    
    inline void syncDevice()
    {
        write(fd_sync_device_, attr_sync_, strlen(attr_sync_));
    }

    inline void syncOffset(uint32_t offset)
    {
        char attr[128];
        
        sprintf(attr, "%d", offset);
        write(fd_sync_offset_, attr, strlen(attr));
    }

    inline void syncSize(uint32_t size)
    {
        char attr[128];
        
        sprintf(attr, "%d", size);
        write(fd_sync_size_, attr, strlen(attr));
    }
};

class DMACtrl
{
public:
    int fd_;
    uint32_t* dma_;
    uint32_t count_, dmaSize_;
    const uint32_t command_;
    Buffer sgBuf_;
    uint32_t nSGBuf_;
    std::vector<uint32_t> tailPtrRing_; 
    uint32_t nextTailPtr_;

    DMACtrl(const std::string& name) : command_(1), sgBuf_("udmabuf1", false), nSGBuf_(0), nextTailPtr_(0)
    {
        if ((fd_ = open(("/dev/"+name).c_str(), O_RDWR | O_SYNC)) == -1) 
        {
            printf("%s", ("Unable to open /dev/" + name + "\n").c_str());
            exit(1);
        }

        int fd;
        if ((fd  = open(("/sys/class/uio/" + name + "/maps/map0/size").c_str(), O_RDONLY)) != -1) 
        {
            char buffer[128];
            read(fd, buffer, 127);
            sscanf(buffer, "%x", &dmaSize_);
            close(fd);
        }
        else
        {
            printf("Unable to open phys_addr\n");
            exit(1);        
        }

        dma_=(uint32_t*) (mmap(0,dmaSize_,PROT_READ|PROT_WRITE,MAP_SHARED, fd_, 0));
        if (dma_ == (uint32_t*)-1)
        {
            printf("Unable to memory map dma\n");
            exit(1);
        }
    }

    void configureSGBuf(const Buffer& buf, const uint32_t length, const uint32_t sa)
    {
        tailPtrRing_.clear();
        nextTailPtr_ = 0;
        nSGBuf_ = buf.size()/length;
        for(unsigned int i = 0; i < sgBuf_.size()/4; ++i) sgBuf_[i] = 0;
        for(unsigned int i = 0; i < nSGBuf_; ++i)
        {
            sgBuf_[0 + i*128/4] = sgBuf_.physAddr() + 128*((i+1)%nSGBuf_);
            sgBuf_[2 + i*128/4] = sa;
            sgBuf_[4 + i*128/4] = buf.physAddr() + length*i;
            sgBuf_[6 + i*128/4] = length;

            tailPtrRing_.push_back(sgBuf_.physAddr() + 128*(i%nSGBuf_));
        }
        curdescPtr(sgBuf_.physAddr());
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

    inline uint32_t nextTaildescPtr()
    {
        dma_[4] = tailPtrRing_[nextTailPtr_];
        uint32_t ntailPtr = nextTailPtr_;
        nextTailPtr_ = (nextTailPtr_ + 1)%nSGBuf_;
        return ntailPtr;
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
        
        read(fd_, &count_, 4);

        IRQReset();
    }

    inline void IRQReset()
    {
        dma_[1] = 1 << 12;
        write(fd_, &command_, 4);
    }

    inline void transfer(uint32_t sa, uint32_t da, uint32_t size)
    {
        dma_[0x18/4] = sa;
        dma_[0x20/4] = da;
        dma_[0x28/4] = size;
    }

    ~DMACtrl()
    {
        munmap(dma_, dmaSize_);

        close(fd_);
    }
};


int main()
{
    const int READS_PER_TRIGGER = 128 + 1;
    const int NUM_TRIGGERS = 1000;

    uint32_t buff[READS_PER_TRIGGER*8];

    UIO gpio("axi-gpio-0");
    gpio[0]=042;

    DMACtrl dma_ctrl("uio0");
    dma_ctrl.reset();

    usleep(1000);
    
    dma_ctrl.keyholeRead(true);
    dma_ctrl.IRQEnable(true);
    dma_ctrl.IRQReset();

    Buffer dma_buf("udmabuf0");
    dma_buf.syncSize(READS_PER_TRIGGER*8*4);
    dma_buf.syncOffset(0);

    dma_buf.syncCpu();
    for(int i = 0; i < dma_buf.size()/4; ++i) dma_buf[i] = 0;

    uint32_t chipMask = 0x0f;

    LTC2333 ltc;

    ltc.configure(0xff, 0);
    ltc.setReadDepth(READS_PER_TRIGGER);
    ltc.setReadPeriod(0);

    while(ltc.writeInProgress()) usleep(10);

    ltc.reset();

    ltc.enableRead(true);
    
    ltc.trigger();
    while(!ltc.fifoReady(0, READS_PER_TRIGGER)) usleep(10);

//    printf("cr: %8x\nsr: %8x\n", dma_ctrl.dma_[0], dma_ctrl.dma_[1]);

    for(int iTrig = 0; iTrig < NUM_TRIGGERS; ++iTrig)
    {
        while(ltc.writeInProgress()) usleep(10);
        ltc.trigger();

        //for(int i = 0; i < READS_PER_TRIGGER*8; ++i) dma_buf[i] = 0;
        for(int i = 0; i < 8; ++i)
        {
            if((1 << i) & chipMask)
            {
        	ltc.wait(i, READS_PER_TRIGGER);

                dma_buf.syncDevice();
                dma_ctrl.transfer(0x76000000 + 0x10000*i, dma_buf.physAddr()+i*READS_PER_TRIGGER*sizeof(uint32_t), READS_PER_TRIGGER*sizeof(uint32_t));
                dma_ctrl.IRQWait();
                dma_ctrl.IRQReset();
                dma_buf.syncCpu();

        	memcpy(buff+i*(READS_PER_TRIGGER - 1), dma_buf.mem()+1+i*READS_PER_TRIGGER, (READS_PER_TRIGGER-1)*sizeof(uint32_t));
            }
        }
    }
    
    //usleep(10000);
    for(int i = 0; i < 4; ++i)
    {
        std::cout << "FIFOOcc: " << i << "\t" << ltc.getFIFOOcc(i) << std::endl;
    }
    
    for(int i = 0; i < READS_PER_TRIGGER - 1; ++i)
    {
        std::cout << i << "\t" << (0x7&(buff[i] >> 3))
        	       << "\t" << (0x7&(buff[i + READS_PER_TRIGGER - 1] >> 3))
        	       << "\t" << (0x7&(buff[i + 2*(READS_PER_TRIGGER - 1)] >> 3))
        	       << "\t" << (0x7&(buff[i + 3*(READS_PER_TRIGGER - 1)] >> 3)) << std::endl;
    }
    ltc.enableRead(false);
}
