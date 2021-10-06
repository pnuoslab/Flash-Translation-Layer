/**
 * @file page-map.c
 * @brief manage the mapping information
 * @author Gijun Oh
 * @version 1.0
 * @date 2021-10-05
 */
#include "include/bits.h"
#include "include/page.h"
#include "include/device.h"
#include "include/log.h"
#include "include/atomic.h"

#include <errno.h>

/**
 * @brief get page from the segment
 *
 * @param pgftl pointer of the page-ftl structure
 *
 * @return free space's device address
 */
struct device_address page_ftl_get_free_page(struct page_ftl *pgftl)
{
	struct device_address paddr;
	struct device *dev;

	struct page_ftl_segment *segment;

	size_t nr_segments;
	size_t pages_per_segment;
	size_t segnum;
	size_t idx;

	uint64_t nr_free_pages;
	uint64_t nr_valid_pages;
	uint32_t offset;

	dev = pgftl->dev;
	nr_segments = device_get_nr_segments(dev);
	pages_per_segment = device_get_pages_per_segment(dev);

	paddr.lpn = PADDR_EMPTY;
	idx = 0;

retry:
	if (idx == nr_segments) {
		pr_err("cannot find the free page in the device\n");
		paddr.lpn = PADDR_EMPTY;
		return paddr;
	}
	segnum = (pgftl->alloc_segnum + idx) % nr_segments;
	idx += 1;

	segment = &pgftl->segments[segnum];
	nr_free_pages = atomic_load(&segment->nr_free_pages);
	if (nr_free_pages == 0) {
		goto retry;
	}
	pgftl->alloc_segnum = segnum;

	offset = find_first_zero_bit(segment->use_bits, pages_per_segment, 0);
	if (offset == (uint32_t)BITS_NOT_FOUND) {
		pr_warn("nr_free_pages and use_bits bitmap are not synchronized(nr_free_pages: %lu, offset: %u)\n",
			nr_free_pages, offset);
		goto retry;
	}
	paddr.lpn = 0;
	paddr.format.block = segnum;
	paddr.lpn |= offset;

	set_bit(segment->use_bits, offset);
	atomic_store(&segment->nr_free_pages, nr_free_pages - 1);

	nr_valid_pages = atomic_load(&segment->nr_valid_pages);
	atomic_store(&segment->nr_valid_pages, nr_valid_pages + 1);

	return paddr;
}

int page_ftl_update_map(struct page_ftl *pgftl, uint64_t sector, uint32_t ppn)
{
	uint32_t *trans_map;
	uint64_t lpn;
	size_t map_size;

	lpn = page_ftl_get_lpn(pgftl, sector);

	map_size = page_ftl_get_map_size(pgftl) / sizeof(uint32_t);
	if (lpn >= (uint64_t)map_size) {
		pr_err("lpn value overflow detected (max: %zu, cur: %lu)\n",
		       map_size, lpn);
		return -EINVAL;
	}

	trans_map = pgftl->trans_map;
	trans_map[lpn] = ppn;

	return 0;
}

struct device_address page_ftl_get_map(struct page_ftl *pgftl, uint64_t sector)
{
	struct device_address paddr;
	uint64_t lpn;
	size_t map_size;

	lpn = page_ftl_get_lpn(pgftl, sector);
	map_size = page_ftl_get_map_size(pgftl) / sizeof(uint32_t);
	if (lpn >= (uint64_t)map_size) {
		pr_err("lpn value overflow detected (max: %zu, cur: %lu)\n",
		       map_size, lpn);
		paddr.lpn = PADDR_EMPTY;
		return paddr;
	}
	paddr.lpn = pgftl->trans_map[lpn];

	return paddr;
}
