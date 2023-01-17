#include "UIO.h"

#include <sys/mman.h>
#include <filesystem>
#include <regex>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <iostream>

UIO::UIO(const std::string& instance_id) : fd_(-100)
{
    fd_ = openByInstance_id(instance_id);

    if(fd_ >= 0)
    {
	memptr_ = (uint32_t*) (mmap(0,0x1000,PROT_READ|PROT_WRITE,MAP_SHARED, fd_, 0x0));
    }
    else
    {
	//ERROR
	std::cout << "ERROR" << std::endl;
    }
}

UIO::UIO(const uint32_t& addr)
{
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

int UIO::openByInstance_id(const std::string& instance_id)
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
		    int fd = open(("/dev/" + dir_entry.path().stem().string()).c_str(), O_RDWR | O_SYNC);
		    return fd;
		}
	    }
	}
    }
    
//    FILE* f = fopen("/sys/class/uio/uio8/device/of_node/target_labels", "r");
//    char * line = NULL;
//    size_t len = 0;
//    ssize_t read;
//    while ((read = getline(&line, &len, f)) != -1)
//    {
//        printf("Retrieved line of length %zu:\n", read);
//	char *loc = line;
//	while((loc - line) < read)
//	{
//	    printf("%i, %s\n", loc-line, loc);
//	    loc = strchr(loc, '\0') + 1;
//	}
//    }
//    fclose(f);
    
    return 0;
}
