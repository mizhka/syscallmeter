#include <emmintrin.h>
#include <stdint.h>

#ifdef _MSC_VER
#	include <intrin.h>
#	pragma intrinsic(__rdtscp, _mm_lfence)
#elif defined(__GNUC__)
#   include <x86intrin.h>
#endif

#	if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
		// MSC or GCC on Intel
		static inline uint64_t vi_tmGetTicks(void)
		{	uint32_t _; // будет удалён оптимизатором
			const uint64_t result = __rdtscp(&_);
			_mm_lfence();
			return result;
		}

		static inline uint64_t vi_tmPreGetTicks(void)
		{   uint32_t _; // будет удалён оптимизатором
			_mm_mfence();
			_mm_lfence();
			const uint64_t result = __rdtscp(&_);
			return result;
		}
#	elif __ARM_ARCH >= 8 // ARMv8 (RaspberryPi4)
		static inline uint64_t vi_tmGetTicks(void)
		{	uint64_t result;
			__asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(result));
			return result;
		}
#	elif defined(_WIN32) // Windows on other platforms.
		static inline uint64_t vi_tmGetTicks(void)
		{	LARGE_INTEGER cnt;
			QueryPerformanceCounter(&cnt);
			return cnt.QuadPart;
		}
#	elif defined(__linux__) // Linux on other platforms
		static inline uint64_t vi_tmGetTicks(void)
		{	struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
			return 1000000000ULL * ts.tv_sec + ts.tv_nsec;
		}
#	else
#		error "You need to define function(s) for your OS and CPU"
#	endif
