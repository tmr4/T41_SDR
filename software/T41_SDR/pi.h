#undef  round
#undef  PI
#undef  HALF_PI
#undef  TWO_PI

// these are defined in wiring.h but for faster calculations on Teensy 4.1 use float literals.
// The FPU performs 32 bit float and 64 bit double precision math in hardware.
// 32 bit float speed is approximately the same speed as integer math. 64 bit
// double precision runs at half the speed of 32 bit float.
// https://www.pjrc.com/store/teensy41.html
#define PI        3.1415926535897932384626433832795f
#define HALF_PI   1.5707963267948966192313216916398f
#define TWO_PI    6.283185307179586476925286766559f
#define FOURPI    (2.0f * TWO_PI)
#define SIXPI     (3.0f * TWO_PI)
