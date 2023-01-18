#include "UIO.h"
#include "LTC2333.h"

#include <iostream>

int main()
{
    UIO gpio("axi-gpio-0");
    gpio[0]=000;

    uint32_t chipMask = 0x0f;

    LTC2333 ltc;

    ltc.configure(0xff);
    ltc.setReadDepth(17);

    ltc.enableRead(true);
    ltc.reset();

    ltc.trigger();

    for(int i = 0; i < 8; ++i)
    {
	if((1 << i) & chipMask)
	{
	    std::cout << "Chip: " << i << std::endl;
	    auto data = ltc.read(i, 17);

	    for(const auto& datum : data)
	    {
		std::cout << datum << std::endl;
	    }
	}
    }
    
}
