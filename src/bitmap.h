#include <string.h>
#include <limits.h>

#define PART_BYTES_PER_LONG ((int)sizeof(unsigned long))
#define PART_BITS_PER_BYTE  (8)
#define PART_BITS_PER_LONG  (PART_BYTES_PER_LONG*8)

/*
 * Safe conversion from bits to bytes with overflow checking.
 * Returns 0 on overflow to indicate error (0 is not a valid bitmap size).
 */
static inline unsigned long long BITS_TO_BYTES(unsigned long long bits)
{
	/* Check for overflow: if bits > ULLONG_MAX - 7, then bits + 7 will overflow */
	if (bits > ULLONG_MAX - (PART_BITS_PER_BYTE - 1))
		return 0;
	return (bits + PART_BITS_PER_BYTE - 1) / PART_BITS_PER_BYTE;
}

/*
 * Safe conversion from bits to longs with overflow checking.
 * Returns 0 on overflow to indicate error (0 is not a valid bitmap size).
 */
static inline unsigned long long BITS_TO_LONGS(unsigned long long bits)
{
	/* Check for overflow: if bits > ULLONG_MAX - (PART_BITS_PER_LONG-1), overflow occurs */
	if (bits > ULLONG_MAX - (PART_BITS_PER_LONG - 1))
		return 0;
	return (bits + PART_BITS_PER_LONG - 1) / PART_BITS_PER_LONG;
}

static inline int
pc_test_bit(unsigned long int nr, unsigned long *bitmap,
	    unsigned long long total)
{
	if (!bitmap)
		return -1;
	if (nr >= total){
	    printf("test block %lu out of boundary(%llu)\n", nr, total);
		exit(1);
	}
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	return (bitmap[offset] >> bit) & 1;
}

static inline void
pc_set_bit(unsigned long int nr, unsigned long *bitmap,
	   unsigned long long total)
{
	if (!bitmap)
		return;
	if (nr >= total){
	    printf("set block %lu out of boundary(%llu)\n", nr, total);
		exit(1);
	}
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	bitmap[offset] |= 1UL << bit;
}

static inline void
pc_clear_bit(unsigned long int nr, unsigned long *bitmap,
	     unsigned long long total)
{
	if (!bitmap)
		return;
	if (nr >= total){
	    printf("clear block %lu out of boundary(%llu)\n", nr, total);
		exit(1);
	}
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	bitmap[offset] &= ~(1UL << bit);
}

static inline unsigned long* pc_alloc_bitmap(unsigned long bits)
{
	unsigned long long num_longs = BITS_TO_LONGS(bits);
	/* Check for overflow - BITS_TO_LONGS returns 0 on overflow */
	if (num_longs == 0 && bits != 0)
		return NULL;
	return (unsigned long*)calloc(PART_BYTES_PER_LONG, num_longs);
}

static inline void pc_init_bitmap(unsigned long* bitmap, char value, unsigned long bits)
{
	unsigned long long num_longs = BITS_TO_LONGS(bits);
	/* Check for overflow - BITS_TO_LONGS returns 0 on overflow */
	if (num_longs == 0 && bits != 0)
		return;
	unsigned long byte_count = PART_BYTES_PER_LONG * num_longs;

	memset(bitmap, value, byte_count);
}
