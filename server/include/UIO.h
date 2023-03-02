#ifndef UIO_H
#define UIO_H

#include <stdint.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>

class UIO
{
public:
    UIO(const std::string& instance_id);
    UIO(const uint32_t& addr);

    ~UIO();

    void write(const uint32_t reg, const uint32_t value);

    uint32_t read(const uint32_t reg);

    inline int fd()
    {
        return fd_;
    }

    inline void IRQWait()
    {
        ::read(fd_, &count_, 4);
    }

    inline void IRQReset()
    {
        ::write(fd_, &IRQCommand_, 4);
    }
    
    inline volatile uint32_t& operator[](const uint32_t& reg)
    {
	return memptr_[reg];
    }

    inline uint32_t operator[](const uint32_t& reg) const
    {
	return memptr_[reg];
    }

private:
    void openByInstance_id(const std::string& instance_id);
    
    volatile uint32_t * memptr_;
    int fd_, count_;
    unsigned int size_;
    static constexpr uint32_t IRQCommand_ = 1;
	
};

#endif
