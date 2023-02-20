#ifndef UDMABUF_H
#define UDMABUF_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string>
#include <cstring>

const char attr_sync_[2] = "1";

class UDMABuf
{
    int fd_, fd_sync_device_, fd_sync_cpu_, fd_sync_offset_, fd_sync_size_;
    uint32_t *mem_;
    uint32_t physAddr_;
    uint32_t size_;
    const std::string name_;

public:

    UDMABuf(const std::string& name, bool cache = true) : name_(name)
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

    ~UDMABuf()
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

#endif
