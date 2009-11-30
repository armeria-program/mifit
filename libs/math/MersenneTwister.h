#ifndef mi_math_MersenneTwister_h
#define mi_math_MersenneTwister_h

#include <cstdlib>
#include <climits>
#include <cfloat>
#include <math/Random.h>

#ifdef _MSC_VER
#include <ctime>
#define int64_t _int64
#else
#include <sys/time.h>
#endif

namespace mi
{
    namespace math
    {

/**
 *  Mersenne Twister random number generator.
 *
 *  It has a Mersenne prime period of 2^19937 - 1 (about 10^6000) and
 *  is equi-distributed in 623 dimensions. It has passed the DIEHARD
 *  statistical tests. It uses 624 words of state per generator and is
 *  comparable in speed to the other generators.
 *
 *  Details of the algorithm are in the paper: Makoto Matsumoto and
 *  Takuji Nishimura, "Mersenne Twister: A 623-dimensionally
 *  equidistributerd uniform pseudorandom number generator". ACM
 *  Transactions on Modeling and Computer Simulation, Vol. 8, No. 1,
 *  Jan. 1998, Pages 3-30
 *
 *  Makoto Matsumoto has a web page with more information about the
 *  generator, http://www.math.keio.ac.jp/~matumoto/emt.html. You can
 *  obtain the paper directly from Makoto Matsumoto's web page.
 *
 *  The period of this generator is 2^{19937} - 1.
 */

#define MT_SIZE 624

        class MersenneTwister : public Random
        {

            /**
             *  Period parameters
             */
            const int N;

            /**
             *  Period parameters
             */
            const int M;

            /**
             *  Most significant w-r bits
             */
            const unsigned int UPPER_MASK;

            /**
             *  Least significant r bits
             */
            const unsigned int LOWER_MASK;

            /**
             *  The seed used.
             */
            unsigned int seed;

            /**
             *  State variables.
             */
            unsigned int mt[MT_SIZE];

            /**
             *  State variables.
             */
            int mti;

            static unsigned int MAGIC(unsigned int y)
            {
                if (y & 0x1)
                {
                    return 0x9908b0dfU;
                }
                else
                {
                    return 0;
                }
            }

            /**
             *  Restarts the random number generator with the current seed.
             */
            void reset()
            {

                mt[0] = seed & 0xffffffffU;

                // We use the congruence s_{n+1} = (69069*s_n) mod 2^32 to
                // initialize the state. This works because ANSI-C unsigned int
                // integer arithmetic is automatically modulo 2^32 (or a higher
                // power of two), so we can safely ignore overflow.

                int i;
                for (i = 1; i < N; i++)
                {
                    mt[i] = ((69069 * mt[i - 1]) & 0xffffffffU);
                }

                mti = i;
            }

        protected:

            /**
             *  Returns an integer with <i>bits</i> bits filled
             *  with a random number.
             *
             *  @param bits the number bits int the return
             *  value. A valid argument is between 1 and
             *  sizeof(int)*CHAR_BIT-1.
             *
             *  @return a integer with a value in the range of the
             *  given number of bits
             */
            virtual unsigned int next(int bits)
            {
                unsigned int k;

                if (mti >= N)
                {
                    /* generate N words at one time */
                    int kk;
                    for (kk = 0; kk < N - M; kk++)
                    {
                        unsigned int y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
                        mt[kk] = mt[kk + M] ^ (y >> 1) ^ MAGIC(y);
                    }
                    for (; kk < N - 1; kk++)
                    {
                        unsigned int y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
                        mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ MAGIC(y);
                    }

                    unsigned int y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
                    mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ MAGIC(y);

                    mti = 0;
                }

                /* Tempering */

                k = mt[mti];
                k ^= (k >> 11);
                k ^= (k << 7) & 0x9d2c5680U;
                k ^= (k << 15) & 0xefc60000U;
                k ^= (k >> 18);

                mti++;

                return (k >> (32-bits));
            }

        public:
            /**
             *  Creates the random number generator and automatically seeds
             *  based on the time.
             */
            MersenneTwister() : N(MT_SIZE), M(397), UPPER_MASK(0x80000000U), LOWER_MASK(0x7fffffffU)
            {
                unsigned int timeSeed;
#ifdef _MSC_VER
                timeSeed = rand();
#else
                struct timezone tzp;
                struct timeval tp;

                gettimeofday(&tp, &tzp);
                timeSeed = tp.tv_usec;
#endif
                setSeed(timeSeed);
            }

            /**
             *  Creates the random number generator with the seed given.  If
             *  the generator is seeded with the same value of s on two
             *  different runs, the same stream of random numbers will be
             *  generated by successive calls to the routines below. If
             *  different values of s are supplied, then the generated streams
             *  of random numbers should be completely different.
             *
             *  @param s the random number seed.
             */
            MersenneTwister(unsigned int s)  : N(MT_SIZE), M(397), UPPER_MASK(0x80000000U), LOWER_MASK(0x7fffffffU)
            {
                setSeed(s);
            }

            /**
             * Sets the seed of this random number generator.
             *
             * @param seed the seed.
             */
            virtual void setSeed(unsigned int seed)
            {
                this->seed = seed;
                resetGaussian();
                reset();
            }

            /**
             * Returns the seed used to set the state of this
             * random number generator.
             */
            virtual unsigned int getSeed()
            {
                return seed;
            }

            /**
             *  Returns a random integer from the generator. The minimum and
             *  maximum values depend on the algorithm used, but all integers
             *  in the range [min,max] are equally likely. The values of min
             *  and max can determined using the auxiliary functions
             *  getIntMaximum() and getIntMinimum().
             */
            virtual int nextInt()
            {
                return (int)next(31);
            }

            /**
             *  Returns the maximum integer random number.
             */
            virtual int getIntMaximum()
            {
                return (int)((1U << 31)-1U);
            }

            /**
             *  Returns the minimum integer random number.
             */
            virtual int getIntMinimum()
            {
                return 0;
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>boolean</code> value.
             */
            virtual bool nextBoolean()
            {
                return next(1) != 0;
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>float</code> value between
             * <code>0.0</code> and <code>1.0</code> exclusive.
             */
            virtual float nextFloat()
            {
                return nextFloatExclusive();
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>float</code> value between
             * <code>0.0</code> and <code>1.0</code> exclusive.
             */
            virtual float nextFloatExclusive()
            {
                int i = next(24);
                return i / (float)(1 << 24);
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>float</code> value between
             * <code>0.0</code> and <code>1.0</code> inclusive.
             */
            virtual float nextFloatInclusive()
            {
                int i = next(24);
                return i / (float)((1 << 24)-1);
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>double</code> value between
             * <code>0.0</code> and <code>1.0</code> exclusive.
             */
            virtual double nextDouble()
            {
                return nextDoubleExclusive();
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>double</code> value between
             * <code>0.0</code> and <code>1.0</code> exclusive.
             */
            virtual double nextDoubleExclusive()
            {
                int64_t value = ((int64_t)next(26) << 27) + next(27);
                return value / (double)((int64_t)1 << 53);
            }

            /**
             * Returns the next random, uniformly
             * distributed <code>double</code> value between
             * <code>0.0</code> and <code>1.0</code> inclusive.
             */
            virtual double nextDoubleInclusive()
            {
                int64_t value = ((int64_t)next(26) << 27) + next(27);
                return value / (double)(((int64_t)1 << 53) - (int64_t)1);
            }

        };

    }
}


#endif // ifndef mi_math_MersenneTwister_h
