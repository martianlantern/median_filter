#include <algorithm>
#include <cstdlib>
using namespace std;

void median_filterv1(const float *input, float *output, int ny, int nx, int hy, int hx) {

    float *pixels = (float *)malloc((2 * hy + 1) * (2 * hx + 1) * sizeof(float));

    for(int y=0; y<ny; y++) {
        for(int x=0; x<nx; x++) {

            int len = 0;
			for(int i=max(y - hy, 0); i<min(y + hy + 1, ny); i++) {
				for(int j=max(x - hx, 0); j<min(x + hx + 1, nx); j++) {
					pixels[len++] = input[nx*i + j];
				}
			}

            // Sort the pixels
            sort(pixels, pixels + len);

            const int mid = len / 2;

            if (len % 2 == 1) {
                output[nx*y + x] = pixels[mid];
            } else {
                output[nx*y + x] = 0.5f * (pixels[mid] + pixels[mid - 1]);
            }

        }
    }

	free(pixels);

}