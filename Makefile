# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# Use C++17 for inline variables
FLAGS += -std=c++17

# Source files to compile
SOURCES += src/plugin.cpp
SOURCES += src/AcidSeq.cpp

# Add res directory to distributables
DISTRIBUTABLES += res

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
