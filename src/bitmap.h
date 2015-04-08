#define PART_BITS_PER_LONG	((int)sizeof(unsigned long)*8)
#define LONGS(bits)	(((bits)+PART_BITS_PER_LONG-1)/PART_BITS_PER_LONG)

static inline int pc_test_bit(unsigned long int nr, unsigned long *bitmap)
{
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG-1);
	return (bitmap[offset] >> bit) & 1;
}

static inline void pc_set_bit(unsigned long int nr, unsigned long *bitmap)
{
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG-1);
	bitmap[offset] |= 1UL << bit;
}

static inline void pc_clear_bit(unsigned long int nr, unsigned long *bitmap)
{
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG-1);
	bitmap[offset] &= ~(1UL << bit);
}
