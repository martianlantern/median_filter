#include <algorithm>
#include <cstdlib>
using namespace std;

void median_filterv2(const float *input, float *output, int ny, int nx, int hy, int hx) {

    float *pixels = (float *)malloc((2 * hy + 1) * (2 * hx + 1) * sizeof(float));

    for(int y=0; y<ny; y++) {
        for(int x=0; x<nx; x++) {

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
                output[nx*y + x] = pixels[mid];
            } else {
                // even count and need the two middle values
                // find the max in the lower half
                float hi = pixels[mid];
                float lo = pixels[mid - 1];
				for(float *p=pixels; p<pixels + mid - 1; p++) lo = max(lo, *p);
                output[nx*y + x] = 0.5f * (lo + hi);

            }

        }
    }

	free(pixels);

}