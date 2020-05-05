#
# 1. General Compiler Settings
#
COMPILER = gcc
CFLAGS   = -std=c++11 -Wextra -fpermissive -fmessage-length=0 -mbmi2 -mavx2 -MMD -MP -Wno-deprecated-declarations
LDFLAGS  = -lstdc++ -lm
INCLUDES =

#
# 2. Traget Specific Settings
#

# 2.1 Linux / Windows
ifeq ($(shell uname),Linux)
	# TensorRT
	LDFLAGS  += -L/usr/local/cuda/targets/x86_64-linux/lib/ -lpthread -lcudart -lnvinfer -lnvonnxparser -lnvparsers
	INCLUDES += -I/usr/local/cuda/include -I/usr/local/cuda/targets/x86_64-linux/include
	OUTFILE  = AQ
else
	echo 'TensorRT7 on Windows deos not support MinGW. Use MSVC instead.'
endif

# 2.2 Set FLAGS
ifeq ($(TARGET),)
	CFLAGS   += -Ofast -fno-fast-math
endif
ifeq ($(TARGET),debug)
	CFLAGS   += -g -Og
endif

#
# 3. Default Settings
#
OUTFILE  = AQ
OBJDIR   = ./obj
SRCDIR   = ./src
SOURCES  = $(wildcard $(SRCDIR)/*.cc)

OBJECTS  = $(addprefix $(OBJDIR)/, $(SOURCES:.cc=.o))
DEPENDS  = $(OBJECTS:.o=.d)

#
# 4. Public Targets
#
.PHONY: all debug clean
all:
	$(MAKE) executable

debug:
	$(MAKE) TARGET=$@ executable

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(OUTFILE) ${OBJECTS:.o=.gcda}

#
# 5. Private Targets
#
.PHONY: executable
executable: $(OBJECTS)
	$(COMPILER) -o $(OUTFILE) $^ $(LDFLAGS) $(CFLAGS)

$(OBJDIR)/%.o: %.cc Makefile
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(COMPILER) $(CFLAGS) $(INCLUDES) -o $@ -c $<

-include $(DEPENDS)

