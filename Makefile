CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Wpedantic
LDFLAGS ?= 

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -liconv
endif

TARGET = bgi-extractor
SRCS = src/main.cpp src/arc.cpp src/dsc.cpp src/parser.cpp src/decompressor.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
