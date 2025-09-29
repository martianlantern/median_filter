#include <algorithm>
#include <cstdlib>
using namespace std;

constexpr int Nx = 8;
constexpr int Ny = 4;

void median_filterv3(const float *input, float *output, int ny, int nx, int hy, int hx) {

    int Sx = nx / Nx + 1;
    int Sy = ny / Ny + 1;

    // Split the image into blocks of size Sx x Sy
    // and process each block in parallel
    // yg -> grid index of the block in y
    // xg -> grid index of the block in x
	#pragma omp parallel for collapse(2) schedule(dynamic)
    for(int yg=0; yg<ny; yg+=Sy) {
        for(int xg=0; xg<nx; xg+=Sx) {

            float *pixels = (float *)malloc((2 * hy + 1) * (2 * hx + 1) * sizeof(float));

            for(int y=yg; y-yg<Sy && y<ny; y++) {
                for(int x=xg; x-xg<Sx && x<nx; x++) {

                    int len = 0;
                    for(int i=max(y - hy, 0); i<min(y + hy + 1, ny); i++) {
                        for(int j=max(x - hx, 0); j<min(x + hx + 1, nx); j++) {
                            pixels[len++] = input[nx*i + j];
                        }
                    }
        
                    const int mid = len / 2;

                    // Move the element in the middle as if the array was sorted
                    nth_element(pixels, pixels + mid, pixels + len);

                    if (len & 1) {
                        // odd count and mid is the median
                        output[nx* y + x] = pixels[mid];
                    } else {
                        // even count and need the two middle values
                        // find the max in the lower half
                        float hi = pixels[mid];
                        float lo = pixels[mid - 1];
                        for(float *p=pixels; p<pixels + mid - 1; p++) lo = max(lo, *p);
                        output[nx* y + x] = 0.5f * (lo + hi);

                    }

                }
            }

            free(pixels);

        }
    }

}