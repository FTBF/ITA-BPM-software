#ifndef UIO_H
#define UIO_H

#include <stdint.h>
#include <string>

class UIO
{
public:
    UIO(const std::string& instance_id);
    UIO(const uint32_t& addr);

    ~UIO();

    void write(const uint32_t reg, const uint32_t value);

    uint32_t read(const uint32_t reg);

    inline volatile uint32_t& operator[](const uint32_t& reg)
    {
	return memptr_[reg];
    }

    inline uint32_t operator[](const uint32_t& reg) const
    {
	return memptr_[reg];
    }

private:
    int openByInstance_id(const std::string& instance_id);
    
    volatile uint32_t * memptr_;
    int fd_;
	
};

#endif
