# Median Filter Benchmark

This benchmark system tests the accuracy and performance of different median filter implementations.

## Current Implementations

### Float Versions (32-bit floating point)
- **v1**: Basic implementation with full sorting
- **v2**: Uses nth_element optimization  
- **v3**: Parallel OpenMP version
- **v4**: Optimized bit manipulation version (x86_64 only)

### Uint8 Versions (8-bit integer images)
- **v5**: Histogram-based median filter optimized for 8-bit images

## Quick Start

```bash
# Build and run the benchmark
make run

# Or build and run separately
make
./benchmark
```

## Features

- **Accuracy Testing**: Compares all implementations against a reference implementation
- **Multiple Test Patterns**: Random, gradient, checkerboard, noise spikes, constant
- **Various Image Sizes**: From 32x32 to 256x256 pixels
- **Different Kernel Sizes**: From 3x3 to 9x9 filters
- **Extensible Design**: Easy to add new implementations

## Adding New Versions

The benchmark supports both **float** and **uint8_t** versions. From v5 onwards, we use uint8_t for 8-bit images.

### Adding a Float Version

To add a new float median filter version (e.g., v6):

1. **Create your implementation** in `mfv6.cc`:
   ```cpp
   void median_filterv6(const float *input, float *output, int ny, int nx, int hy, int hx) {
       // Your float implementation here
   }
   ```

2. **Update the Makefile** - add `mfv6.cc` to `SOURCES`

3. **Register in benchmark.cc**:
   ```cpp
   // Add extern declaration
   extern void median_filterv6(const float *input, float *output, int ny, int nx, int hy, int hx);
   
   // Add registration in constructor
   registerFloatVersion("v6", median_filterv6, "Your description here");
   ```

### Adding a Uint8 Version

To add a new uint8_t median filter version (e.g., v7):

1. **Create your implementation** in `mfv7.cc`:
   ```cpp
   void median_filterv7(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx) {
       // Your uint8_t implementation here (0-255 values)
   }
   ```

2. **Update the Makefile** - add `mfv7.cc` to `SOURCES`

3. **Register in benchmark.cc**:
   ```cpp
   // Add extern declaration
   extern void median_filterv7(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx);
   
   // Add registration in constructor
   registerUint8Version("v7", median_filterv7, "Your description here");
   ```

4. **Rebuild and test**:
   ```bash
   make clean
   make run
   ```

## Function Signatures

Median filter implementations must follow one of these signatures:

### Float Version
```cpp
void median_filterv{N}(const float *input, float *output, int ny, int nx, int hy, int hx)
```

### Uint8 Version  
```cpp
void median_filterv{N}(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx)
```

Where:
- `input`: Input image data (row-major flattened array)
- `output`: Output image data (row-major flattened array)  
- `ny`: Image height in pixels
- `nx`: Image width in pixels
- `hy`: Half-height of kernel (full kernel height = 2*hy + 1)
- `hx`: Half-width of kernel (full kernel width = 2*hx + 1)

The benchmark automatically tests each version with appropriate data types and reference implementations.

## Compilation Requirements

- C++17 compatible compiler
- OpenMP support (for parallel versions)
- x86-64 architecture (for v4's bit manipulation features)

## Output Interpretation

The benchmark reports:
- **Status**: PASS/FAIL for accuracy
- **Max Error**: Largest pixel difference from reference
- **Mean Error**: Average pixel difference
- **RMSE**: Root mean square error
- **Diff Pixels**: Number of pixels that differ significantly

A version "PASSES" if all pixel differences are within tolerance (1e-5 by default).
