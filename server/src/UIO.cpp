#include "UIO.h"

#include <filesystem>
#include <regex>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <cstdio>

UIO::UIO(const std::string& instance_id) : fd_(-100), size_(0)
{
    //set fd_ and size_
    openByInstance_id(instance_id);

    if(fd_ >= 0)
    {
	memptr_ = (uint32_t*) (mmap(0,size_,PROT_READ|PROT_WRITE,MAP_SHARED, fd_, 0x0));
    }
    else
    {
	//ERROR
	std::cout << "ERROR" << std::endl;
    }

    pfds_ = reinterpret_cast<pollfd*>(calloc(1, sizeof(struct pollfd)));
    pfds_[0].fd = fd_;
    pfds_[0].events = POLLIN;
}

UIO::UIO(const uint32_t& addr)
{
    (void)addr;
}

UIO::~UIO()
{
    if(fd_ >= 0) close(fd_);
}

void UIO::write(const uint32_t reg, const uint32_t value)
{
    memptr_[reg] = value;
}

uint32_t UIO::read(const uint32_t reg)
{
    return memptr_[reg];
}

void UIO::openByInstance_id(const std::string& instance_id)
{
    const std::filesystem::path uiodir{"/sys/class/uio"};

    for (auto const& dir_entry : std::filesystem::directory_iterator{uiodir}) 
    {
	std::filesystem::directory_entry id_path(dir_entry.path()/"device/of_node/instance_id");
	if(id_path.exists())
	{
	    FILE* f = fopen(id_path.path().string().c_str(), "r");
	    if(f != NULL)
	    {
		char buff[256];
		fgets(buff, 256, f);
		if(strcmp(buff, instance_id.c_str()) == 0)
		{
                    std::string uioName = dir_entry.path().stem().string();
		    fd_ = open(("/dev/" + uioName).c_str(), O_RDWR | O_SYNC);

                    int fd;
                    if ((fd  = open(("/sys/class/uio/" + uioName + "/maps/map0/size").c_str(), O_RDONLY)) != -1) 
                    {
                        char buffer[128];
                        ::read(fd, buffer, 127);
                        sscanf(buffer, "%x", &size_);
                        close(fd);
                    }
                    else
                    {
                        printf("Unable to open phys_addr\n");
                        size_ = 0x1000;
                    }
                    return;
		}
	    }
	}
    }

    printf("No uio device found with instance_it \"%s\"\n", instance_id.c_str());
}
