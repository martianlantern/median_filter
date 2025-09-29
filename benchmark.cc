#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <random>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <map>
#include <cstdlib>

// Function pointer types for different data types
typedef void (*MedianFilterFuncFloat)(const float *input, float *output, int ny, int nx, int hy, int hx);
typedef void (*MedianFilterFuncUint8)(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx);

// Enum for data types
enum class DataType {
    FLOAT,
    UINT8
};

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

// Registry for median filter versions
struct FilterVersion {
    std::string name;
    DataType dataType;
    union {
        MedianFilterFuncFloat floatFunc;
        MedianFilterFuncUint8 uint8Func;
    } func;
    std::string description;
};

class MedianFilterBenchmark {
private:
    std::vector<FilterVersion> versions_;
    std::mt19937 rng_;
    
public:
    MedianFilterBenchmark() : rng_(42) {  // Fixed seed for reproducibility
        // Register all versions - easily extensible!
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
    
    // Easy way to add new float versions
    void registerFloatVersion(const std::string& name, MedianFilterFuncFloat func, const std::string& description) {
        FilterVersion version;
        version.name = name;
        version.dataType = DataType::FLOAT;
        version.func.floatFunc = func;
        version.description = description;
        versions_.push_back(version);
    }
    
    // Easy way to add new uint8 versions
    void registerUint8Version(const std::string& name, MedianFilterFuncUint8 func, const std::string& description) {
        FilterVersion version;
        version.name = name;
        version.dataType = DataType::UINT8;
        version.func.uint8Func = func;
        version.description = description;
        versions_.push_back(version);
    }
    
    // Reference implementation for ground truth (uses standard library sort)
    void referenceMedianFilter(const float *input, float *output, int ny, int nx, int hy, int hx) {
        std::vector<float> pixels((2 * hy + 1) * (2 * hx + 1));
        
        for(int y = 0; y < ny; y++) {
            for(int x = 0; x < nx; x++) {
                int len = 0;
                
                // Extract neighborhood pixels
                for(int i = std::max(y - hy, 0); i < std::min(y + hy + 1, ny); i++) {
                    for(int j = std::max(x - hx, 0); j < std::min(x + hx + 1, nx); j++) {
                        pixels[len++] = input[nx * i + j];
                    }
                }
                
                // Sort and find median
                std::sort(pixels.begin(), pixels.begin() + len);
                const int mid = len / 2;
                
                if (len % 2 == 1) {
                    output[nx * y + x] = pixels[mid];
                } else {
                    output[nx * y + x] = 0.5f * (pixels[mid] + pixels[mid - 1]);
                }
            }
        }
    }
    
    // Reference implementation for uint8_t
    void referenceMedianFilterUint8(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx) {
        std::vector<uint8_t> pixels((2 * hy + 1) * (2 * hx + 1));
        
        for(int y = 0; y < ny; y++) {
            for(int x = 0; x < nx; x++) {
                int len = 0;
                
                // Extract neighborhood pixels
                for(int i = std::max(y - hy, 0); i < std::min(y + hy + 1, ny); i++) {
                    for(int j = std::max(x - hx, 0); j < std::min(x + hx + 1, nx); j++) {
                        pixels[len++] = input[nx * i + j];
                    }
                }
                
                // Sort and find median
                std::sort(pixels.begin(), pixels.begin() + len);
                const int mid = len / 2;
                
                if (len % 2 == 1) {
                    output[nx * y + x] = pixels[mid];
                } else {
                    // For uint8, use proper rounding
                    output[nx * y + x] = (pixels[mid] + pixels[mid - 1] + 1) / 2;
                }
            }
        }
    }
    
    // Generate test image with different patterns (float version)
    std::vector<float> generateTestImageFloat(int ny, int nx, const std::string& pattern) {
        std::vector<float> image(ny * nx);
        
        if (pattern == "random") {
            std::uniform_real_distribution<float> dist(0.0f, 255.0f);
            for(int i = 0; i < ny * nx; i++) {
                image[i] = dist(rng_);
            }
        }
        else if (pattern == "gradient") {
            for(int y = 0; y < ny; y++) {
                for(int x = 0; x < nx; x++) {
                    image[y * nx + x] = (float)(x + y) * 255.0f / (nx + ny - 2);
                }
            }
        }
        else if (pattern == "checkerboard") {
            for(int y = 0; y < ny; y++) {
                for(int x = 0; x < nx; x++) {
                    image[y * nx + x] = ((x + y) % 2 == 0) ? 0.0f : 255.0f;
                }
            }
        }
        else if (pattern == "noise_spikes") {
            std::uniform_real_distribution<float> base_dist(100.0f, 150.0f);
            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            for(int i = 0; i < ny * nx; i++) {
                if (prob_dist(rng_) < 0.1f) {  // 10% spikes
                    image[i] = (prob_dist(rng_) < 0.5f) ? 0.0f : 255.0f;
                } else {
                    image[i] = base_dist(rng_);
                }
            }
        }
        else if (pattern == "constant") {
            std::fill(image.begin(), image.end(), 128.0f);
        }
        
        return image;
    }
    
    // Generate test image with different patterns (uint8 version)
    std::vector<uint8_t> generateTestImageUint8(int ny, int nx, const std::string& pattern) {
        std::vector<uint8_t> image(ny * nx);
        
        if (pattern == "random") {
            std::uniform_int_distribution<int> dist(0, 255);
            for(int i = 0; i < ny * nx; i++) {
                image[i] = static_cast<uint8_t>(dist(rng_));
            }
        }
        else if (pattern == "gradient") {
            for(int y = 0; y < ny; y++) {
                for(int x = 0; x < nx; x++) {
                    image[y * nx + x] = static_cast<uint8_t>((x + y) * 255 / (nx + ny - 2));
                }
            }
        }
        else if (pattern == "checkerboard") {
            for(int y = 0; y < ny; y++) {
                for(int x = 0; x < nx; x++) {
                    image[y * nx + x] = ((x + y) % 2 == 0) ? 0 : 255;
                }
            }
        }
        else if (pattern == "noise_spikes") {
            std::uniform_int_distribution<int> base_dist(100, 150);
            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
            for(int i = 0; i < ny * nx; i++) {
                if (prob_dist(rng_) < 0.1f) {  // 10% spikes
                    image[i] = (prob_dist(rng_) < 0.5f) ? 0 : 255;
                } else {
                    image[i] = static_cast<uint8_t>(base_dist(rng_));
                }
            }
        }
        else if (pattern == "constant") {
            std::fill(image.begin(), image.end(), 128);
        }
        
        return image;
    }
    
    // Compare two images and return statistics
    struct ComparisonStats {
        double maxError;
        double meanError;
        double rmse;
        int differentPixels;
        bool isAccurate;
    };
    
    ComparisonStats compareImagesFloat(const std::vector<float>& reference, 
                                     const std::vector<float>& test,
                                     double tolerance = 1e-5) {
        ComparisonStats stats = {0.0, 0.0, 0.0, 0, true};
        
        double sumError = 0.0;
        double sumSquaredError = 0.0;
        
        for(size_t i = 0; i < reference.size(); i++) {
            double error = std::abs(reference[i] - test[i]);
            stats.maxError = std::max(stats.maxError, error);
            sumError += error;
            sumSquaredError += error * error;
            
            if (error > tolerance) {
                stats.differentPixels++;
                stats.isAccurate = false;
            }
        }
        
        stats.meanError = sumError / reference.size();
        stats.rmse = std::sqrt(sumSquaredError / reference.size());
        
        return stats;
    }
    
    ComparisonStats compareImagesUint8(const std::vector<uint8_t>& reference, 
                                      const std::vector<uint8_t>& test,
                                      int tolerance = 0) {
        ComparisonStats stats = {0.0, 0.0, 0.0, 0, true};
        
        double sumError = 0.0;
        double sumSquaredError = 0.0;
        
        for(size_t i = 0; i < reference.size(); i++) {
            double error = std::abs(static_cast<int>(reference[i]) - static_cast<int>(test[i]));
            stats.maxError = std::max(stats.maxError, error);
            sumError += error;
            sumSquaredError += error * error;
            
            if (error > tolerance) {
                stats.differentPixels++;
                stats.isAccurate = false;
            }
        }
        
        stats.meanError = sumError / reference.size();
        stats.rmse = std::sqrt(sumSquaredError / reference.size());
        
        return stats;
    }
    
    // Run accuracy test for a specific configuration
    void testConfiguration(int ny, int nx, int hy, int hx, const std::string& pattern) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Testing Configuration:" << std::endl;
        std::cout << "  Image Size: " << ny << " x " << nx << std::endl;
        std::cout << "  Kernel Size: " << (2*hy+1) << " x " << (2*hx+1) << std::endl;
        std::cout << "  Pattern: " << pattern << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        // Test each version
        std::cout << std::setw(10) << "Version" 
                 << std::setw(10) << "Type"
                 << std::setw(15) << "Status"
                 << std::setw(15) << "Max Error"
                 << std::setw(15) << "Mean Error"
                 << std::setw(15) << "RMSE"
                 << std::setw(15) << "Diff Pixels"
                 << std::setw(20) << "Description" << std::endl;
        std::cout << std::string(120, '-') << std::endl;
        
        for(const auto& version : versions_) {
            try {
                if (version.dataType == DataType::FLOAT) {
                    // Generate float test data
                    auto input = generateTestImageFloat(ny, nx, pattern);
                    std::vector<float> reference(ny * nx);
                    std::vector<float> testOutput(ny * nx);
                    
                    // Compute reference
                    referenceMedianFilter(input.data(), reference.data(), ny, nx, hy, hx);
                    
                    // Execute the filter
                    version.func.floatFunc(input.data(), testOutput.data(), ny, nx, hy, hx);
                    
                    // Compare with reference
                    auto stats = compareImagesFloat(reference, testOutput);
                    
                    std::cout << std::setw(10) << version.name
                             << std::setw(10) << "float"
                             << std::setw(15) << (stats.isAccurate ? "PASS" : "FAIL")
                             << std::setw(15) << std::scientific << std::setprecision(2) << stats.maxError
                             << std::setw(15) << stats.meanError
                             << std::setw(15) << stats.rmse
                             << std::setw(15) << stats.differentPixels
                             << std::setw(20) << version.description.substr(0, 19)
                             << std::endl;
                             
                } else if (version.dataType == DataType::UINT8) {
                    // Generate uint8 test data
                    auto input = generateTestImageUint8(ny, nx, pattern);
                    std::vector<uint8_t> reference(ny * nx);
                    std::vector<uint8_t> testOutput(ny * nx);
                    
                    // Compute reference
                    referenceMedianFilterUint8(input.data(), reference.data(), ny, nx, hy, hx);
                    
                    // Execute the filter
                    version.func.uint8Func(input.data(), testOutput.data(), ny, nx, hy, hx);
                    
                    // Compare with reference
                    auto stats = compareImagesUint8(reference, testOutput);
                    
                    std::cout << std::setw(10) << version.name
                             << std::setw(10) << "uint8"
                             << std::setw(15) << (stats.isAccurate ? "PASS" : "FAIL")
                             << std::setw(15) << std::scientific << std::setprecision(2) << stats.maxError
                             << std::setw(15) << stats.meanError
                             << std::setw(15) << stats.rmse
                             << std::setw(15) << stats.differentPixels
                             << std::setw(20) << version.description.substr(0, 19)
                             << std::endl;
                }
                         
            } catch(const std::exception& e) {
                std::cout << std::setw(10) << version.name
                         << std::setw(10) << (version.dataType == DataType::FLOAT ? "float" : "uint8")
                         << std::setw(15) << "ERROR"
                         << "  Exception: " << e.what() << std::endl;
            } catch(...) {
                std::cout << std::setw(10) << version.name
                         << std::setw(10) << (version.dataType == DataType::FLOAT ? "float" : "uint8")
                         << std::setw(15) << "ERROR"
                         << "  Unknown exception" << std::endl;
            }
        }
    }
    
    // Run comprehensive benchmark
    void runBenchmark() {
        std::cout << "Median Filter Accuracy Benchmark" << std::endl;
        std::cout << "=================================" << std::endl;
        std::cout << "Registered versions: " << versions_.size() << std::endl;
        for(const auto& version : versions_) {
            std::cout << "  " << version.name << ": " << version.description << std::endl;
        }
        
        // Test different image sizes
        std::vector<std::pair<int, int>> imageSizes = {
            {32, 32},    // Small
            {64, 64},    // Medium
            {128, 128},  // Large
            {100, 150},  // Non-square
            {256, 256}   // Very large
        };
        
        // Test different kernel sizes
        std::vector<std::pair<int, int>> kernelSizes = {
            {1, 1},   // 3x3
            {2, 2},   // 5x5
            {3, 3},   // 7x7
            {1, 2},   // 3x5 (non-square)
            {4, 4}    // 9x9
        };
        
        // Test different patterns
        std::vector<std::string> patterns = {
            "random",
            "gradient",
            "checkerboard",
            "noise_spikes",
            "constant"
        };
        
        // Run tests (subset to avoid too much output)
        for(const auto& pattern : patterns) {
            for(const auto& imgSize : {std::make_pair(64, 64), std::make_pair(128, 128)}) {
                for(const auto& kernelSize : {std::make_pair(1, 1), std::make_pair(2, 2), std::make_pair(3, 3)}) {
                    testConfiguration(imgSize.first, imgSize.second, 
                                    kernelSize.first, kernelSize.second, pattern);
                }
            }
        }
        
        std::cout << "\nBenchmark completed!" << std::endl;
        std::cout << "\nTo add a new version (e.g., v5):" << std::endl;
        std::cout << "1. Implement median_filterv5() function" << std::endl;
        std::cout << "2. Add extern declaration at the top of benchmark.cc" << std::endl;
        std::cout << "3. Add registerVersion(\"v5\", median_filterv5, \"Description\") in constructor" << std::endl;
    }
};

// Compile all versions into the benchmark
// You'll need to compile this with all your median filter source files:
// g++ -fopenmp -O3 -march=native benchmark.cc mfv1.cc mfv2.cc mfv3.cc mfv4.cc -o benchmark

int main() {
    MedianFilterBenchmark benchmark;
    benchmark.runBenchmark();
    return 0;
}
