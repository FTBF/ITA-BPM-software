#include "UIO.h"
#include "LTC2333_write.h"
#include "LTC2333_read.h"

#include <iostream>

int main()
{
    UIO gpio("axi-gpio-0");
    gpio[0]=0;

    uint32_t chipMask = 0xff;

    std::vector<LTC2333_read> ltc_read;
    for(int ichip = 0; ichip < 8; ++ ichip)
    {
	if((1 << ichip) & chipMask) ltc_read.emplace_back(ichip);
    }
    
    LTC2333_write ltc_write;

    ltc_write.configure(0xff);
    ltc_write.setReadDepth(17);

    ltc_read[0].configure(true);
    ltc_read[0].reset();

    ltc_write.trigger();

    for(int i = 0; i < 8; ++i)
    {
	if((1 << i) & chipMask)
	{
	    std::cout << "Chip: " << i << std::endl;
	    auto data = ltc_read[i].read(17);

	    for(const auto& datum : data)
	    {
		std::cout << datum << std::endl;
	    }
	}
    }
    
}
