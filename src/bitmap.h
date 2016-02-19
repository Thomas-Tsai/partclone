#define PART_BITS_PER_LONG	((int)sizeof(unsigned long)*8)
#define LONGS(bits)	(((bits)+PART_BITS_PER_LONG-1)/PART_BITS_PER_LONG)

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
