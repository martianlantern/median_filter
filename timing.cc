#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <algorithm>

// Function pointer types for different data types
typedef void (*MedianFilterFuncFloat)(const float *input, float *output, int ny, int nx, int hy, int hx);
typedef void (*MedianFilterFuncUint8)(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx);

// Include all the median filter versions
extern void median_filterv1(const float *input, float *output, int ny, int nx, int hy, int hx);
extern void median_filterv2(const float *input, float *output, int ny, int nx, int hy, int hx);
extern void median_filterv3(const float *input, float *output, int ny, int nx, int hy, int hx);

// v4 is only available on x86_64 architectures
#ifdef HAVE_MFV4
extern void median_filterv4(const float *input, float *output, int ny, int nx, int hy, int hx);
#endif

// v5+ use uint8_t
extern void median_filterv5(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx);

// OpenCV implementations (if available)
#ifdef HAVE_OPENCV
extern void median_filter_opencv_float(const float *input, float *output, int ny, int nx, int hy, int hx);
extern void median_filter_opencv_uint8(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx);
#endif

// Enum for data types
enum class DataType {
    FLOAT,
    UINT8
};

// Structure to hold version information
struct FilterVersion {
    std::string name;
    DataType dataType;
    union {
        MedianFilterFuncFloat floatFunc;
        MedianFilterFuncUint8 uint8Func;
    } func;
    std::string description;
};

// Structure to hold timing results
struct TimingResult {
    std::string version;
    int kernelSize;
    double meanTime;
    double stdTime;
    double minTime;
    double maxTime;
};

class MedianFilterTimer {
private:
    std::vector<FilterVersion> versions_;
    std::mt19937 rng_;
    
public:
    MedianFilterTimer() : rng_(42) {  // Fixed seed for reproducibility
        // Register all versions
        registerFloatVersion("v1", median_filterv1, "Basic implementation with full sorting");
        registerFloatVersion("v2", median_filterv2, "Uses nth_element optimization");
        registerFloatVersion("v3", median_filterv3, "Parallel OpenMP version");
        
        // v4 is only available on x86_64 architectures
#ifdef HAVE_MFV4
        registerFloatVersion("v4", median_filterv4, "Optimized bit manipulation version");
#endif

        // v5+ use uint8_t
        registerUint8Version("v5", median_filterv5, "Histogram-based median for 8-bit images");
        
        // OpenCV implementations (if available)
#ifdef HAVE_OPENCV
        registerFloatVersion("opencv", median_filter_opencv_float, "OpenCV medianBlur (float)");
        registerUint8Version("opencv", median_filter_opencv_uint8, "OpenCV medianBlur (uint8)");
#endif
    }
    
    void registerFloatVersion(const std::string& name, MedianFilterFuncFloat func, const std::string& description) {
        FilterVersion version;
        version.name = name;
        version.dataType = DataType::FLOAT;
        version.func.floatFunc = func;
        version.description = description;
        versions_.push_back(version);
    }
    
    void registerUint8Version(const std::string& name, MedianFilterFuncUint8 func, const std::string& description) {
        FilterVersion version;
        version.name = name;
        version.dataType = DataType::UINT8;
        version.func.uint8Func = func;
        version.description = description;
        versions_.push_back(version);
    }
    
    // Generate random test image (float version)
    std::vector<float> generateTestImageFloat(int ny, int nx) {
        std::vector<float> image(ny * nx);
        std::uniform_real_distribution<float> dist(0.0f, 255.0f);
        
        for(int i = 0; i < ny * nx; i++) {
            image[i] = dist(rng_);
        }
        
        return image;
    }
    
    // Generate random test image (uint8 version)
    std::vector<uint8_t> generateTestImageUint8(int ny, int nx) {
        std::vector<uint8_t> image(ny * nx);
        std::uniform_int_distribution<int> dist(0, 255);
        
        for(int i = 0; i < ny * nx; i++) {
            image[i] = static_cast<uint8_t>(dist(rng_));
        }
        
        return image;
    }
    
    // Time a single run of a filter
    double timeFilter(const FilterVersion& version, int ny, int nx, int hy, int hx, int runs = 5) {
        std::vector<double> times;
        
        for(int run = 0; run < runs; run++) {
            if (version.dataType == DataType::FLOAT) {
                auto input = generateTestImageFloat(ny, nx);
                std::vector<float> output(ny * nx);
                
                auto start = std::chrono::high_resolution_clock::now();
                version.func.floatFunc(input.data(), output.data(), ny, nx, hy, hx);
                auto end = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                times.push_back(duration.count() / 1000.0);  // Convert to milliseconds
                
            } else if (version.dataType == DataType::UINT8) {
                auto input = generateTestImageUint8(ny, nx);
                std::vector<uint8_t> output(ny * nx);
                
                auto start = std::chrono::high_resolution_clock::now();
                version.func.uint8Func(input.data(), output.data(), ny, nx, hy, hx);
                auto end = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                times.push_back(duration.count() / 1000.0);  // Convert to milliseconds
            }
        }
        
        // Return median time to reduce noise
        std::sort(times.begin(), times.end());
        return times[times.size() / 2];
    }
    
    // Calculate statistics for timing results
    TimingResult calculateStats(const std::string& versionName, int kernelSize, const std::vector<double>& times) {
        TimingResult result;
        result.version = versionName;
        result.kernelSize = kernelSize;
        
        // Calculate mean
        double sum = 0.0;
        for(double time : times) {
            sum += time;
        }
        result.meanTime = sum / times.size();
        
        // Calculate standard deviation
        double variance = 0.0;
        for(double time : times) {
            variance += (time - result.meanTime) * (time - result.meanTime);
        }
        result.stdTime = std::sqrt(variance / times.size());
        
        // Min and max
        result.minTime = *std::min_element(times.begin(), times.end());
        result.maxTime = *std::max_element(times.begin(), times.end());
        
        return result;
    }
    
    // Run timing benchmark
    std::vector<TimingResult> runTimingBenchmark(int ny = 500, int nx = 500, int runs = 5) {
        std::vector<TimingResult> results;
        
        // Test different kernel sizes (hy, hx pairs)
        std::vector<std::pair<int, int>> kernelSizes = {
            {1, 1},   // 3x3
            {2, 2},   // 5x5
            {3, 3},   // 7x7
            {4, 4},   // 9x9
            {5, 5},   // 11x11
            {6, 6},   // 13x13
            {7, 7},   // 15x15
            {8, 8},   // 17x17
            {9, 9},   // 19x19
            {10, 10}  // 21x21
        };
        
        std::cout << "Running timing benchmark..." << std::endl;
        std::cout << "Image size: " << ny << " x " << nx << std::endl;
        std::cout << "Runs per configuration: " << runs << std::endl;
        std::cout << std::endl;
        
        // Progress tracking
        int totalTests = versions_.size() * kernelSizes.size();
        int currentTest = 0;
        
        for(const auto& version : versions_) {
            std::cout << "Testing " << version.name << " (" << version.description << ")" << std::endl;
            
            for(const auto& kernel : kernelSizes) {
                int hy = kernel.first;
                int hx = kernel.second;
                int kernelSize = 2 * hy + 1;  // Full kernel size for display
                
                currentTest++;
                std::cout << "  Kernel " << kernelSize << "x" << kernelSize 
                         << " (" << currentTest << "/" << totalTests << ")... " << std::flush;
                
                std::vector<double> times;
                
                // Warm-up run (not counted)
                timeFilter(version, ny, nx, hy, hx, 1);
                
                // Actual timing runs
                for(int run = 0; run < runs; run++) {
                    double time = timeFilter(version, ny, nx, hy, hx, 1);
                    times.push_back(time);
                }
                
                TimingResult result = calculateStats(version.name, kernelSize, times);
                results.push_back(result);
                
                std::cout << std::fixed << std::setprecision(2) 
                         << result.meanTime << "ms ±" << result.stdTime << "ms" << std::endl;
            }
            std::cout << std::endl;
        }
        
        return results;
    }
    
    // Save results to CSV file
    void saveResultsToCSV(const std::vector<TimingResult>& results, const std::string& filename) {
        std::ofstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << " for writing" << std::endl;
            return;
        }
        
        // CSV header
        file << "Version,KernelSize,MeanTime,StdTime,MinTime,MaxTime" << std::endl;
        
        // Data rows
        for(const auto& result : results) {
            file << result.version << ","
                 << result.kernelSize << ","
                 << std::fixed << std::setprecision(6) << result.meanTime << ","
                 << result.stdTime << ","
                 << result.minTime << ","
                 << result.maxTime << std::endl;
        }
        
        file.close();
        std::cout << "Results saved to " << filename << std::endl;
    }
    
    // Print summary table
    void printSummary(const std::vector<TimingResult>& results) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "TIMING BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << std::setw(10) << "Version" 
                 << std::setw(12) << "Kernel Size"
                 << std::setw(15) << "Mean (ms)"
                 << std::setw(15) << "Std Dev (ms)"
                 << std::setw(15) << "Min (ms)"
                 << std::setw(15) << "Max (ms)" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for(const auto& result : results) {
            std::cout << std::setw(10) << result.version
                     << std::setw(12) << (std::to_string(result.kernelSize) + "x" + std::to_string(result.kernelSize))
                     << std::setw(15) << std::fixed << std::setprecision(2) << result.meanTime
                     << std::setw(15) << result.stdTime
                     << std::setw(15) << result.minTime
                     << std::setw(15) << result.maxTime << std::endl;
        }
    }
};

// Generate Python plotting script
void generatePlotScript(const std::string& csvFile) {
    std::ofstream script("plot_timing.py");
    
    if (!script.is_open()) {
        std::cerr << "Error: Could not create plot_timing.py" << std::endl;
        return;
    }
    
    script << R"(#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read the CSV data
df = pd.read_csv(')" << csvFile << R"(')

# Create the plot
plt.figure(figsize=(12, 8))

# Get unique versions and assign colors
versions = df['Version'].unique()
colors = plt.cm.Set1(np.linspace(0, 1, len(versions)))

for i, version in enumerate(versions):
    version_data = df[df['Version'] == version]
    
    plt.errorbar(version_data['KernelSize'], version_data['MeanTime'], 
                yerr=version_data['StdTime'],
                label=version, marker='o', capsize=5, 
                color=colors[i], linewidth=2, markersize=6)

plt.xlabel('Kernel Size (pixels)', fontsize=12, fontweight='bold')
plt.ylabel('Average Runtime (ms)', fontsize=12, fontweight='bold')
plt.title('Median Filter Performance Comparison\n500x500 Image, 5 Runs Average', 
          fontsize=14, fontweight='bold')
plt.legend(fontsize=11, loc='upper left')
plt.grid(True, alpha=0.3)
plt.yscale('log')  # Log scale for better visualization of different performance levels

# Customize the plot
plt.tight_layout()

# Save the plot
plt.savefig('median_filter_timing.png', dpi=300, bbox_inches='tight')
plt.savefig('median_filter_timing.pdf', bbox_inches='tight')

print("Plot saved as 'median_filter_timing.png' and 'median_filter_timing.pdf'")

# Show the plot
plt.show()
)";
    
    script.close();
    std::cout << "Python plotting script generated: plot_timing.py" << std::endl;
}

int main() {
    std::cout << "Median Filter Timing Benchmark" << std::endl;
    std::cout << "==============================" << std::endl;
    
    MedianFilterTimer timer;
    
    // Run timing benchmark
    auto results = timer.runTimingBenchmark(500, 500, 5);
    
    // Save results
    timer.saveResultsToCSV(results, "timing_results.csv");
    
    // Print summary
    timer.printSummary(results);
    
    // Generate plotting script
    generatePlotScript("timing_results.csv");
    
    std::cout << "\nTo generate the plot, run:" << std::endl;
    std::cout << "python3 plot_timing.py" << std::endl;
    std::cout << "\nMake sure you have matplotlib and pandas installed:" << std::endl;
    std::cout << "pip3 install matplotlib pandas" << std::endl;
    
    return 0;
}
