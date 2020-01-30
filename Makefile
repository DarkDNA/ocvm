ifeq ($(lua),)
	lua=5.2
endif

TARGET_EXEC ?= ocvm

INC_DIRS ?= ./
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

ifneq ($(luapath),)
	LDFLAGS?=${luapath}/src/liblua.a
	INC_FLAGS+=-I${luapath}/src
else
	LDFLAGS?=$(shell pkg-config lua$(lua) --libs 2>/dev/null || pkg-config lua5.3 --libs 2>/dev/null || echo -llua5.2)
	INC_FLAGS+=$(shell pkg-config lua$(lua) --cflags 2>/dev/null || pkg-config lua5.3 --cflags 2>/dev/null || echo -I/usr/include/lua5.2)
endif

ifneq ($(prof),)
	LDFLAGS+=-L../gperftools-2.5/.libs/ -lprofiler
	TARGET_EXEC:=$(TARGET_EXEC)-profiled
endif

LDFLAGS+=-lstdc++
ifeq ($(shell uname -s 2>/dev/null),Haiku)
	LDFLAGS+=-lnetwork
else
	LDFLAGS+=-lstdc++fs -pthread -ldl
endif

SRC_DIRS ?= ./
SRCS=$(wildcard $(SRC_DIRS)*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)apis/*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)color/*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)components/*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)drivers/*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)io/*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)model/*.cpp)
SRCS+=$(wildcard $(SRC_DIRS)util/*.cpp)
ifeq ($(shell uname -s 2>/dev/null),Haiku)
	SRCS+=$(wildcard $(SRC_DIRS)haiku/*.cpp)
endif

ifeq (, $(shell which wget))
	SRCS := $(filter-out $(SRC_DIRS)drivers/internet_http.cpp,$(SRCS))
endif

BUILD_DIR ?= ./bin
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP -Wall -g --std=c++17 -O0 -Wl,--no-as-needed

$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo done

# c++ source
-include $(DEPS)
$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

system:
	@echo downloading OpenComputers system files
	mkdir -p system
	svn checkout https://github.com/MightyPirates/OpenComputers/trunk/src/main/resources/assets/opencomputers/loot system/loot
	wget https://raw.githubusercontent.com/MightyPirates/OpenComputers/master-MC1.7.10/src/main/resources/assets/opencomputers/lua/machine.lua -O system/machine.lua
	wget https://raw.githubusercontent.com/MightyPirates/OpenComputers/master-MC1.7.10/src/main/resources/assets/opencomputers/lua/bios.lua -O system/bios.lua
	wget https://raw.githubusercontent.com/MightyPirates/OpenComputers/master-MC1.7.10/src/main/resources/assets/opencomputers/font.hex -O system/font.hex

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR) $(TARGET_EXEC) $(TARGET_EXEC)-profiled
