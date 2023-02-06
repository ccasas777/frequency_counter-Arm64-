# Makefile
#
# Date: Jan 5, 2023
#
# Include the user defined variables, if they specified them
#-include config.mk

################################################################################
# Configuration
################################################################################

# Set the shell to BASH
SHELL = /bin/bash

# Allow the user to specify cross-compilation from the command line
CC = aarch64-linux-gnu-g++

# Standard gcc flags for compilation
GLOBAL_CFLAGS = -g -Wall -std=c++17 -Wextra -pthread -Ofast -mcpu=cortex-a53 -march=armv8-a -fmessage-length=0 

# The location where the compiled executables and driver will be stored
OUTPUT_DIR ?= outputs


################################################################################
# Targets
################################################################################

# None of the targets correspond to actual files
.PHONY: all docs clean cross_compiler_check 

# Compile library, and examples in release mode as the default
all: library examples

# Include the specific targets for the examples, library, and driver (the
# includes must go here, so the default target is 'all')
include library/library.mk
include examples/examples.mk

# Make the specified output directory, if it doesn't exist
$(OUTPUT_DIR):
	@mkdir -p $(OUTPUT_DIR)

# Clean up all temporary files
clean: examples_clean library_clean
	rm -rf $(OUTPUT_DIR)

# Check that the specified cross-compiler exists
cross_compiler_check:
ifdef CROSS_COMPILE
ifeq (,$(shell which $(CC)))
	@printf "'$(CC)' was not found in your path.\n"
	@exit 1
endif
endif

