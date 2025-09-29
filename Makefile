# Median Filter Benchmark Makefile

# Detect compiler and set appropriate flags
CXX = g++
TARGET = benchmark
TIMING_TARGET = timing

# Base sources that work on all architectures
FILTER_SOURCES = mfv1.cc mfv2.cc mfv3.cc mfv5.cc

# Architecture-specific sources
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    FILTER_SOURCES += mfv4.cc
    CPPFLAGS += -DHAVE_MFV4
else
    $(warning mfv4.cc requires x86_64 architecture. Current: $(ARCH). Skipping v4.)
endif

# OpenCV detection
OPENCV_TEST := $(shell pkg-config --exists opencv4 && echo "opencv4" || (pkg-config --exists opencv && echo "opencv" || echo "no"))
ifneq ($(OPENCV_TEST),no)
    FILTER_SOURCES += mfopencv.cc
    CPPFLAGS += -DHAVE_OPENCV $(shell pkg-config --cflags $(OPENCV_TEST))
    LDFLAGS += $(shell pkg-config --libs $(OPENCV_TEST))
    $(info Found OpenCV: $(OPENCV_TEST))
else
    $(warning OpenCV not found. Install with: brew install opencv or apt-get install libopencv-dev)
endif

# Update sources with OpenCV if available
BENCHMARK_SOURCES = benchmark.cc $(FILTER_SOURCES)
TIMING_SOURCES = timing.cc $(FILTER_SOURCES)

# Base flags
CXXFLAGS = -O3 -Wall -Wextra -std=c++17

# Try to detect if we can use OpenMP
# On macOS, system clang doesn't support OpenMP by default
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS: try to use OpenMP if available, otherwise warn and continue
    OPENMP_TEST := $(shell echo | $(CXX) -fopenmp -E - 2>/dev/null && echo "yes" || echo "no")
    ifeq ($(OPENMP_TEST),yes)
        CXXFLAGS += -fopenmp
    else
        $(warning OpenMP not available with $(CXX). v3 and v4 may not work correctly.)
        $(warning To get OpenMP support, install with: brew install libomp)
        $(warning Or use a different compiler like: make CXX=g++-13)
    endif
else
    # Linux: assume OpenMP is available
    CXXFLAGS += -fopenmp
endif

# Try to add native optimization if supported
MARCH_TEST := $(shell echo | $(CXX) -march=native -E - 2>/dev/null && echo "yes" || echo "no")
ifeq ($(MARCH_TEST),yes)
    CXXFLAGS += -march=native
endif

# Default target
all: $(TARGET) $(TIMING_TARGET)

# Build the benchmark executable
$(TARGET): $(BENCHMARK_SOURCES)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TARGET) $(BENCHMARK_SOURCES) $(LDFLAGS)

# Build the timing executable
$(TIMING_TARGET): $(TIMING_SOURCES)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TIMING_TARGET) $(TIMING_SOURCES) $(LDFLAGS)

# Run the benchmark
run: $(TARGET)
	./$(TARGET)

# Run timing benchmark
time: $(TIMING_TARGET)
	./$(TIMING_TARGET)

# Debug build
debug: CXXFLAGS = -g -Wall -Wextra -std=c++17 -DDEBUG
debug: all

# Clean build files
clean:
	rm -f $(TARGET) $(TIMING_TARGET) *.csv plot_timing.py *.png *.pdf

# Help target
help:
	@echo "Available targets:"
	@echo "  all     - Build both benchmark and timing executables (default)"
	@echo "  run     - Build and run the accuracy benchmark"
	@echo "  time    - Build and run the timing benchmark"
	@echo "  debug   - Build with debug symbols"
	@echo "  clean   - Remove built files and generated output"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "To add new median filter versions:"
	@echo "1. Create your new implementation file (e.g., mfv6.cc)"
	@echo "2. Add it to FILTER_SOURCES in this Makefile"
	@echo "3. Add extern declaration and registration in benchmark.cc and timing.cc"

# Mark targets that don't create files
.PHONY: all run time debug clean help
