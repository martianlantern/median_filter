#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstring>

#ifdef _OPENMP
#include <omp.h>
#else
// Fallback for systems without OpenMP
inline int omp_get_max_threads() { return 1; }
#endif

// Histogram-based median filter optimized for uint8_t (0-255) values
// Uses sliding window with incremental histogram updates for efficiency

struct HistogramWindow {
    static constexpr int HIST_SIZE = 256;
    int histogram[HIST_SIZE];
    int windowSize;
    int medianPos;
    
    HistogramWindow() : windowSize(0), medianPos(0) {
        std::memset(histogram, 0, sizeof(histogram));
    }
    
    // Add a pixel value to the histogram
    inline void add(uint8_t value) {
        histogram[value]++;
        windowSize++;
    }
    
    // Remove a pixel value from the histogram
    inline void remove(uint8_t value) {
        histogram[value]--;
        windowSize--;
    }
    
    // Find median from current histogram
    uint8_t getMedian() {
        if (windowSize == 0) return 0;
        
        int count = 0;
        
        if (windowSize % 2 == 1) {
            // Odd number of pixels - find the middle element (0-indexed)
            int target = windowSize / 2;
            for (int i = 0; i < HIST_SIZE; i++) {
                count += histogram[i];
                if (count > target) {
                    return static_cast<uint8_t>(i);
                }
            }
        } else {
            // Even number of pixels - average the two middle elements
            int target1 = (windowSize / 2) - 1;  // First middle element (0-indexed)
            int target2 = windowSize / 2;         // Second middle element (0-indexed)
            
            count = 0;
            int val1 = -1, val2 = -1;
            
            for (int i = 0; i < HIST_SIZE; i++) {
                int newCount = count + histogram[i];
                
                // Check if target1 is in this bin
                if (val1 == -1 && newCount > target1) {
                    val1 = i;
                }
                
                // Check if target2 is in this bin
                if (val2 == -1 && newCount > target2) {
                    val2 = i;
                    break;  // We found both values
                }
                
                count = newCount;
            }
            
            // Return average of the two middle values (with proper rounding)
            return static_cast<uint8_t>((val1 + val2 + 1) / 2);
        }
        
        return 0;  // Should never reach here with valid input
    }
    
    // Clear the histogram
    void clear() {
        std::memset(histogram, 0, sizeof(histogram));
        windowSize = 0;
    }
};

// Process a single block of the image
void processBlock(const uint8_t *input, uint8_t *output, 
                 int ny, int nx, int hy, int hx,
                 int y_start, int y_end, int x_start, int x_end) {
    
    HistogramWindow hist;
    
    for (int y = y_start; y < y_end; y++) {
        for (int x = x_start; x < x_end; x++) {
            // Clear histogram for new position
            hist.clear();
            
            // Build histogram for current window
            for (int dy = std::max(y - hy, 0); dy <= std::min(y + hy, ny - 1); dy++) {
                for (int dx = std::max(x - hx, 0); dx <= std::min(x + hx, nx - 1); dx++) {
                    hist.add(input[dy * nx + dx]);
                }
            }
            
            // Compute median and store result
            output[y * nx + x] = hist.getMedian();
        }
    }
}

// Optimized version using row-wise sliding window
void processBlockOptimized(const uint8_t *input, uint8_t *output, 
                          int ny, int nx, int hy, int hx,
                          int y_start, int y_end, int x_start, int x_end) {
    
    HistogramWindow hist;
    
    for (int y = y_start; y < y_end; y++) {
        // Initialize histogram for the first pixel in the row
        hist.clear();
        
        int x = x_start;
        
        // Build initial histogram for first position in row
        for (int dy = std::max(y - hy, 0); dy <= std::min(y + hy, ny - 1); dy++) {
            for (int dx = std::max(x - hx, 0); dx <= std::min(x + hx, nx - 1); dx++) {
                hist.add(input[dy * nx + dx]);
            }
        }
        
        // Process first pixel
        output[y * nx + x] = hist.getMedian();
        
        // Slide window horizontally for remaining pixels in row
        for (x = x_start + 1; x < x_end; x++) {
            // Remove left column of previous window
            int left_col = x - hx - 1;
            if (left_col >= 0) {
                for (int dy = std::max(y - hy, 0); dy <= std::min(y + hy, ny - 1); dy++) {
                    hist.remove(input[dy * nx + left_col]);
                }
            }
            
            // Add right column of new window
            int right_col = x + hx;
            if (right_col < nx) {
                for (int dy = std::max(y - hy, 0); dy <= std::min(y + hy, ny - 1); dy++) {
                    hist.add(input[dy * nx + right_col]);
                }
            }
            
            // Compute median for current position
            output[y * nx + x] = hist.getMedian();
        }
    }
}

void median_filterv5(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx) {
    // For small images or large kernels, use simple approach
    if (nx <= 64 || ny <= 64 || (2*hx+1) * (2*hy+1) > 128) {
        processBlock(input, output, ny, nx, hy, hx, 0, ny, 0, nx);
        return;
    }
    
    // Get number of OpenMP threads
    int num_threads = omp_get_max_threads();
    
    // Calculate optimal block sizes for load balancing
    int target_blocks = std::max(num_threads * 2, 4);
    int blocks_per_dim = std::max(1, (int)std::sqrt(target_blocks));
    
    // Calculate actual block sizes (prefer larger blocks for better sliding window efficiency)
    int By = std::max(64, (ny + blocks_per_dim - 1) / blocks_per_dim);
    int Bx = std::max(64, (nx + blocks_per_dim - 1) / blocks_per_dim);
    
    // Process image in parallel blocks
#ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(dynamic)
#endif
    for (int by = 0; by < ny; by += By) {
        for (int bx = 0; bx < nx; bx += Bx) {
            int y_end = std::min(by + By, ny);
            int x_end = std::min(bx + Bx, nx);
            
            // Use optimized sliding window for larger blocks
            if ((x_end - bx) >= 32) {
                processBlockOptimized(input, output, ny, nx, hy, hx, by, y_end, bx, x_end);
            } else {
                processBlock(input, output, ny, nx, hy, hx, by, y_end, bx, x_end);
            }
        }
    }
}