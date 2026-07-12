TARGET := build/axirc
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.hpp)
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
CXX ?= c++

.PHONY: all build cross-platform clean

all: build

build: $(TARGET)

cross-platform: build

$(TARGET): $(SOURCES) $(HEADERS)
	mkdir -p build
			$(CXX) $(SOURCES) $(CPPFLAGS) -o $(TARGET) $(CXXFLAGS)

clean:
	rm -rf build
