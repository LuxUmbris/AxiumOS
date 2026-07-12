TARGET := build/axirc
TARGET_DIRECTORY := build/targets
TARGET_CONFIGS := $(wildcard targets/*.yml)
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.hpp)
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
CXX ?= c++

.PHONY: all build cross-platform clean

all: build

build: $(TARGET)

cross-platform: build

$(TARGET): $(SOURCES) $(HEADERS) $(TARGET_CONFIGS)
	mkdir -p $(TARGET_DIRECTORY)
	$(CXX) $(SOURCES) $(CPPFLAGS) -o $(TARGET) $(CXXFLAGS)
	cp $(TARGET_CONFIGS) $(TARGET_DIRECTORY)/

clean:
	rm -rf build
