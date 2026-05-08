#include <rng.hpp>

#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL
#define UPPER_MASK 0x80000000UL
#define LOWER_MASK 0x7fffffffUL

class SimpleRandom : public Randomizer {
    public:
        SimpleRandom() {
            uint32_t basic_seed = 0xDEADBEEF;
            set_seed(basic_seed);
        }
        void set_seed(uint64_t seed) {
            this->mt[0] = seed;
            for (mti = 1; mti < N; mti++) {
                this->mt[mti] = (1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
                this->mt[mti] &= 0xffffffffUL;
            }
        }
        uint64_t rand() {
            uint32_t y;
            static const uint32_t mag01[2] = {0x0UL, MATRIX_A};

            if (this->mti >= N) {
                int kk;

                for (kk = 0; kk < N - M; kk++) {
                    y = (this->mt[kk] & UPPER_MASK) | (this->mt[kk + 1] & LOWER_MASK);
                    this->mt[kk] = this->mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
                }
                for (; kk < N - 1; kk++) {
                    y = (this->mt[kk] & UPPER_MASK) | (this->mt[kk + 1] & LOWER_MASK);
                    this->mt[kk] = this->mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
                }
                y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
                this->mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

                this->mti = 0;
            }

            y = this->mt[mti++];
            y ^= (y >> 11);
            y ^= (y << 7) & 0x9d2c5680UL;
            y ^= (y << 15) & 0xefc60000UL;
            y ^= (y >> 18);

            return y;
        }
    private:
        uint32_t mt[N];
        int mti = N + 1;
};
static SimpleRandom random;
uint64_t rand() {
    return random.rand();
}