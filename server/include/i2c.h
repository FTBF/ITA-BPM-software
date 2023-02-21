#ifndef I2C_WRAPPER_H
#define I2C_WRAPPER_H

#include <string>
#include <vector>

#include <errno.h>
#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class I2C
{
public:
    I2C(const std::string& device)
    {
        file_ = open_i2c_device(device);
    }

    ~I2C()
    {
        if(file_ >= 0) close(file_);
    }
    
    std::vector<uint8_t> read(uint8_t addr, uint32_t len)
    {
        std::vector<uint8_t> buf(len, 0);

        setAddress(addr);
        
        if (::read(file_,buf.data(),len) != ssize_t(len)) {
            /* ERROR HANDLING: i2c transaction failed */
            printf("Failed to read from the i2c bus.\n");
//            buffer = g_strerror(errno);
//            printf(buffer);
//            printf("\n\n");

        }

        return buf;
    }

    void write(uint8_t addr, const std::vector<uint8_t>& data)
    {
        setAddress(addr);

        if (::write(file_,data.data(),data.size()) != ssize_t(data.size())) {
            /* ERROR HANDLING: i2c transaction failed */
            printf("Failed to write to the i2c bus.\n");
//            buffer = g_strerror(errno);
//            printf(buffer);
//            printf("\n\n");
        }
    }
private:
    int open_i2c_device(const std::string& device)
    {
        int fd = open(device.c_str(), O_RDWR);
        if (fd == -1)
        {
            perror(device.c_str());
            return -1;
        }
        return fd;
    }

    void setAddress(uint8_t addr)
    {
        if (ioctl(file_,I2C_SLAVE,addr) < 0)
        {
            printf("Failed to acquire bus access and/or talk to slave.\n");
            /* ERROR HANDLING; you can check errno to see what went wrong */
        }
    }

    int file_;
};

#endif
