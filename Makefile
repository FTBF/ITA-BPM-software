CXX           = g++
LD            = g++
CXXFLAGS      = -std=c++17 -O3
LDFLAGS       = 

# directory to put intermediate files 
ODIR       = obj
SDIR       = src
IDIR       = include

## Enable for maximum warning
CXXFLAGS += -Wall -Wextra -Wpedantic

# Flags for generating auto dependancies 
CXXDEPFLAGS = -MMD -MP

PROGRAMS = test

all: mkobj $(PROGRAMS)
	echo $^

mkobj:
	@mkdir -p $(ODIR)

$(ODIR)/%.o : $(SDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXDEPFLAGS) -I$(IDIR) -o $@ -c $<

test: $(ODIR)/test.o $(ODIR)/UIO.o $(ODIR)/LTC2333.o 
	$(LD) $^ -o $@

clean:
	rm -f $(ODIR)/*.o $(ODIR)/*.so $(ODIR)/*.d $(PROGRAMS) core
	rm -rf $(ODIR)

-include $(ODIR)/*.d
