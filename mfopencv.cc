#include <opencv2/opencv.hpp>
#include <cstdint>

// OpenCV median filter implementation for float data
// Note: OpenCV's medianBlur only supports 8U format, so we convert float->uint8->float
void median_filter_opencv_float(const float *input, float *output, int ny, int nx, int hy, int hx) {
    // Convert float input to uint8 Mat (scale from [0,255] range)
    cv::Mat inputMat(ny, nx, CV_8UC1);
    
    // Copy and convert data from float array to uint8 Mat
    for(int y = 0; y < ny; y++) {
        for(int x = 0; x < nx; x++) {
            // Clamp to [0, 255] range and convert to uint8
            float val = input[y * nx + x];
            val = std::max(0.0f, std::min(255.0f, val));
            inputMat.at<uint8_t>(y, x) = static_cast<uint8_t>(std::round(val));
        }
    }
    
    // Apply median filter
    cv::Mat outputMat;
    int kernelSize = 2 * std::max(hx, hy) + 1;  // OpenCV uses square kernels
    
    // Ensure odd kernel size
    if (kernelSize % 2 == 0) {
        kernelSize++;
    }
    
    cv::medianBlur(inputMat, outputMat, kernelSize);
    
    // Copy result back to float array
    for(int y = 0; y < ny; y++) {
        for(int x = 0; x < nx; x++) {
            output[y * nx + x] = static_cast<float>(outputMat.at<uint8_t>(y, x));
        }
    }
}

// OpenCV median filter implementation for uint8_t data
void median_filter_opencv_uint8(const uint8_t *input, uint8_t *output, int ny, int nx, int hy, int hx) {
    // Convert flat array to OpenCV Mat
    cv::Mat inputMat(ny, nx, CV_8UC1);
    
    // Copy data from flat array to Mat
    for(int y = 0; y < ny; y++) {
        for(int x = 0; x < nx; x++) {
            inputMat.at<uint8_t>(y, x) = input[y * nx + x];
        }
    }
    
    // Apply median filter
    cv::Mat outputMat;
    int kernelSize = 2 * std::max(hx, hy) + 1;  // OpenCV uses square kernels
    
    // Ensure odd kernel size
    if (kernelSize % 2 == 0) {
        kernelSize++;
    }
    
    cv::medianBlur(inputMat, outputMat, kernelSize);
    
    // Copy result back to flat array
    for(int y = 0; y < ny; y++) {
        for(int x = 0; x < nx; x++) {
            output[y * nx + x] = outputMat.at<uint8_t>(y, x);
        }
    }
}
