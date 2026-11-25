#include <string.h>
#include <limits.h>
#include <stdio.h>

#define PART_BYTES_PER_LONG ((int)sizeof(unsigned long))
#define PART_BITS_PER_BYTE  (8)
#define PART_BITS_PER_LONG  (PART_BYTES_PER_LONG*8)

/*
 * Safe conversion from bits to bytes with overflow checking.
 * Returns 0 on overflow to indicate error (0 is not a valid bitmap size).
 */
static inline unsigned long long pc_BITS_TO_BYTES(unsigned long long bits)
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
static inline unsigned long long pc_BITS_TO_LONGS(unsigned long long bits)
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
	unsigned long long num_longs = pc_BITS_TO_LONGS(bits);
	/* Check for overflow - BITS_TO_LONGS returns 0 on overflow */
	if (num_longs == 0 && bits != 0)
		return NULL;
	return (unsigned long*)calloc(PART_BYTES_PER_LONG, num_longs);
}

static inline void pc_init_bitmap(unsigned long* bitmap, char value, unsigned long bits)
{
	unsigned long long num_longs = pc_BITS_TO_LONGS(bits);
	/* Check for overflow - BITS_TO_LONGS returns 0 on overflow */
	if (num_longs == 0 && bits != 0)
		return;
	unsigned long byte_count = PART_BYTES_PER_LONG * num_longs;

	memset(bitmap, value, byte_count);
}

/*
 * Big-endian bitmap layout fix marker.
 *
 * Images written by partclone versions before 0.3.55 on big-endian hosts
 * store the first pc_BITS_TO_BYTES(totalblock) bytes of the bitmap buffer.
 * Because the used bits of a partially used trailing unsigned long live in
 * that long's highest-addressed bytes on big-endian hosts, those bits were
 * cut off and are lost in such images.
 *
 * Images produced by 0.3.55+ on big-endian hosts instead store [all
 * complete longs] followed by [the used tail bytes of the final long], so
 * every used block bit survives. Readers use pc_be_bitmap_legacy() to
 * detect pre-fix images and read them with the historic contiguous layout.
 *
 * v1 ("0001") images predate the ptc_version header field and are always
 * treated as legacy, as are images whose version string cannot be parsed.
 */
#define PC_BE_BITMAP_FIX_MAJ 0
#define PC_BE_BITMAP_FIX_MIN 3
#define PC_BE_BITMAP_FIX_REV 55

/*
 * Returns nonzero when the image must be read with the historic (pre-fix)
 * big-endian bitmap layout.
 */
static inline int pc_be_bitmap_legacy(const char *ptc_version, unsigned image_version)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	int maj = 0, min = 0, rev = 0;

	if (image_version < 0x0002)
		return 1;
	if (!ptc_version ||
	    sscanf(ptc_version, "%d.%d.%d", &maj, &min, &rev) != 3)
		return 1;
	if (maj != PC_BE_BITMAP_FIX_MAJ)
		return maj < PC_BE_BITMAP_FIX_MAJ;
	if (min != PC_BE_BITMAP_FIX_MIN)
		return min < PC_BE_BITMAP_FIX_MIN;
	return rev < PC_BE_BITMAP_FIX_REV;
#else
	(void)ptc_version;
	(void)image_version;
	return 0;
#endif
}

/*
 * Bitmap image I/O segmentation.
 *
 * The image stores the bitmap as pc_BITS_TO_BYTES(totalblock) raw bytes.
 *
 * On little-endian hosts - and on big-endian hosts for legacy (pre-0.3.55)
 * images - the layout is one contiguous segment: the first
 * pc_BITS_TO_BYTES(totalblock) bytes of the buffer.
 *
 * On big-endian hosts for images written by 0.3.55+ (legacy == 0), the
 * layout is [all complete longs] followed by [the used tail bytes of the
 * final long], since the used bits of a partially used trailing long live
 * in its highest-addressed bytes. The file size is unchanged:
 * seg1_len + seg2_len == pc_BITS_TO_BYTES(totalblock).
 *
 * Returns the number of segments (1 or 2). With 2 segments, writers must
 * emit (and readers must fill) seg1 then seg2, in order.
 */
static inline int pc_bitmap_image_segments(unsigned long *bitmap,
	unsigned long long totalblock, int legacy,
	char **seg1, unsigned long long *seg1_len,
	char **seg2, unsigned long long *seg2_len)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	unsigned long long full_longs = 0, rem_bits = 0, tail_bytes = 0;

	if (legacy)
		goto contiguous;

	full_longs = totalblock / PART_BITS_PER_LONG;
	rem_bits   = totalblock % PART_BITS_PER_LONG;
	tail_bytes = pc_BITS_TO_BYTES(rem_bits);

	*seg1     = (char*)bitmap;
	*seg1_len = full_longs * PART_BYTES_PER_LONG;
	if (tail_bytes) {
		*seg2     = (char*)&bitmap[full_longs] + (PART_BYTES_PER_LONG - (unsigned long)tail_bytes);
		*seg2_len = tail_bytes;
		return 2;
	}
contiguous:
	*seg1     = (char*)bitmap;
	*seg1_len = pc_BITS_TO_BYTES(totalblock);
	*seg2 = NULL;
	*seg2_len = 0;
	return 1;
#else
	(void)legacy;
	*seg1     = (char*)bitmap;
	*seg1_len = pc_BITS_TO_BYTES(totalblock);
	*seg2 = NULL;
	*seg2_len = 0;
	return 1;
#endif
}
