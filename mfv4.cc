#include <vector>
#include <cstdint>
#include <iostream>
#include <x86intrin.h>
#include <algorithm>
#include <omp.h>
#include <cmath>

struct Block {

    int nx, ny;
    int bx, by;
    int hy, hx;
    int x0b, x1b, y0b, y1b;
    int x0i, y0i, x1i, y1i;
    int x0, y0, x1, y1;
    int words, p;
    int psum[2];
    std::vector<std::pair<float, int>> sorted;
    std::vector<int> ranks;
    std::vector<uint64_t> buff;

    Block(int ny, int nx, int hy, int hx, const float *in, int x0i, int y0i, int x1i, int y1i)
    : nx(nx), ny(ny), hy(hy), hx(hx), x0i(x0i), y0i(y0i), x1i(x1i), y1i(y1i) {

        // The boundaries of the block
        x0b = std::max(x0i - hx, 0);
        y0b = std::max(y0i - hy, 0);
        x1b = std::min(x1i + hx, nx - 1);
        y1b = std::min(y1i + hy, ny - 1);

        x0 = x0i - x0b;
        y0 = y0i - y0b;
        x1 = x1i - x0b;
        y1 = y1i - y0b;

        // The number of pixels in the block along x and y
        bx = (x1b - x0b + 1);
        by = (y1b - y0b + 1);

        sorted.resize(bx * by);
        ranks.resize(bx * by);

        for(int dy=0; dy<by; dy++) for(int dx=0; dx<bx; dx++) {
            sorted[dy * bx + dx] = {in[(y0b + dy) * nx + (x0b + dx)], dy * bx + dx};
        }

        std::sort(
            sorted.begin(), sorted.end(),
            [](auto &a, auto &b){
                return a.first < b.first;
            }
        );
    
        for (int i=0; i<bx * by; i++) {
            ranks[sorted[i].second] = i;
        }

        words = (bx * by + 63) / 64;
		psum[0] = psum[1] = 0;
		p = words / 2;
        buff = std::vector<uint64_t>(words, 0);

    }

    // all indices are local block coordinates
	inline void add_rank(int ix, int jy) {
        if (ix < 0 || ix >= bx || jy < 0 || jy >= by) return;
        int rank = ranks[jy * bx + ix];
		int i = rank >> 6;
		buff[i] ^= (uint64_t(1) << (rank & 63));
		psum[i >= p] += 1;
	}

	inline void remove_rank(int ix, int jy) {
        if (ix < 0 || ix >= bx || jy < 0 || jy >= by) return;
        int rank = ranks[jy * bx + ix];
		int i = rank >> 6;
		buff[i] ^= (uint64_t(1) << (rank & 63));
		psum[i >= p] -= 1;

	}

	inline int pop(int idx) const {
		return __builtin_popcountll(buff[idx]);
	}

    inline int search(int target) {

        // localize the target chunk in buffer
		while (psum[0] > target) {
			p--;
			psum[0] -= pop(p);
			psum[1] += pop(p);
		}
		while (psum[0] + pop(p) <= target) {
			psum[0] += pop(p);
			psum[1] -= pop(p);
			p++;
		}
		int n = target - psum[0];

		// courtesy of:
		// https://stackoverflow.com/questions/7669057/find-nth-set-bit-in-an-int
		uint64_t x = _pdep_u64(uint64_t(1) << n, buff[p]);
		int bitpos = __builtin_ctzll(x);
		return (p << 6) | bitpos;

    }

    inline float get_median() {

        int sum = psum[0] + psum[1];
        int i1 = search((sum - 1) / 2);
        if(sum % 2 == 1) {
            return sorted[i1].first;
        } else {
            int i2 = search(sum / 2);
            return (sorted[i1].first + sorted[i2].first) / 2;
        }

	}

    inline void compute_median(float *out) {

        for(int ix=x0-hx; ix<x0+hx; ix++) for(int jy=y0-hy; jy<=y0+hy; jy++) add_rank(ix, jy);

        int x = x0 - 1;
        while(x <= x1) {

            // Remove the left vertical boundary of the window and add the right
            for(int jy=y0 - hy; jy<=y0 + hy; jy++) remove_rank(x - hx, jy);
            x++; if(x > x1) break;
            for(int jy=y0 - hy; jy<=y0 + hy; jy++) add_rank(x + hx, jy);

            int y = y0;
            while(y < y1) {

                out[(x + x0b) + nx * (y + y0b)] = get_median();
    
                // remove the upper horizontal boundary and add lower
                if(y - hy >= 0) for(int ix=-hx; ix<=hx; ix++) remove_rank(x + ix, y - hy);
                y++;
                if(y + hy < ny) for(int ix=-hx; ix<=hx; ix++) add_rank(x + ix, y + hy);
    
            }
    
            out[(x + x0b) + nx * (y + y0b)] = get_median();
    
            // Remove the left vertical boundary of the window
            for(int jy=y - hy; jy<=y + hy; jy++) remove_rank(x - hx, jy);
            x++; if(x > x1) break;
            // Add rigth vertical boundary of the window
            for(int jy=y - hy; jy<=y + hy; jy++) add_rank(x + hx, jy);
    
            y = y1;
            while(y > y0) {
    
                out[(x + x0b) + nx * (y + y0b)] = get_median();
    
                // remove the lower horizontal boundary
                if(y + hy < ny) for(int ix=-hx; ix<=hx; ix++) remove_rank(x + ix, y + hy);
                y--;
                // add the upper horizontal boundary
                if(y - hy >= 0) for(int ix=-hx; ix<=hx; ix++) add_rank(x + ix, y - hy);
    
            }
    
            out[(x + x0b) + nx * (y + y0b)] = get_median();

        }

    }

};

void median_filterv4(const float *input, float *output, int ny, int nx, int hy, int hx) {

    // Get number of OpenMP threads
    int num_threads = omp_get_max_threads();
    
    // Calculate optimal block sizes for load balancing
    // Target: 2-4 blocks per thread for good load distribution
    int target_blocks = std::max(num_threads * 3, 4);  // At least 4 blocks total
    
    // Calculate block dimensions trying to keep them roughly square
    // while ensuring good thread utilization
    int blocks_per_dim = std::max(1, (int)std::sqrt(target_blocks));
    
    // Calculate actual block sizes
    int Bx = std::max(32, (nx + blocks_per_dim - 1) / blocks_per_dim);  // At least 32 pixels
    int By = std::max(32, (ny + blocks_per_dim - 1) / blocks_per_dim);  // At least 32 pixels
    
    // For very small images, use the entire image as one block
    if (nx <= 64 && ny <= 64) {
        Bx = nx;
        By = ny;
    }
    
    // Ensure we don't create blocks that are too large
    // (this prevents memory issues and maintains reasonable parallelism)
    Bx = std::min(Bx, std::max(nx / 2, 64));
    By = std::min(By, std::max(ny / 2, 64));

    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int y0 = 0; y0 < ny; y0 += By) {
        for (int x0 = 0; x0 < nx; x0 += Bx) {

            int x1 = std::min(x0 + Bx - 1, nx - 1);
            int y1 = std::min(y0 + By - 1, ny - 1);

            Block block = Block(
                ny, nx, hy, hx, input,
                x0, y0, x1, y1
            );
        
            block.compute_median(output);

        }
    }

}