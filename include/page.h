/**
 * @file page.h
 * @brief declaration of data structures and macros for page ftl
 * @author Gijun Oh
 * @version 1.0
 * @date 2021-09-22
 */
#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <pthread.h>

#include <limits.h>
#include <unistd.h>

#include <glib.h>

#include "flash.h"
#include "atomic.h"
#include "device.h"

#define PAGE_FTL_CACHE_SIZE (2)

/**
 * @brief segment information structure
 * @note
 * Segment number is same as block number
 */
struct page_ftl_segment {
	atomic64_t nr_free_pages;
	atomic64_t nr_valid_pages;

	uint64_t *use_bits; /**< contain the use page information */
	GList *lba_list; /**< lba_list which contains the valid data */
};

/**
 * @brief contain the page flash translation layer information
 */
struct page_ftl {
	uint32_t *trans_map; /**< page-level mapping table */
	uint64_t alloc_segnum; /**< last allocated segment number */
	struct page_ftl_segment *segments;
	struct device *dev;
	pthread_mutex_t mutex;
};

/* page-interface.c */
int page_ftl_open(struct page_ftl *);
int page_ftl_close(struct page_ftl *);

ssize_t page_ftl_submit_request(struct page_ftl *, struct device_request *);
ssize_t page_ftl_write(struct page_ftl *, struct device_request *);
ssize_t page_ftl_read(struct page_ftl *, struct device_request *);

int page_ftl_module_init(struct flash_device *, uint64_t flags);
int page_ftl_module_exit(struct flash_device *);

/* page-map.c */
struct device_address page_ftl_get_free_page(struct page_ftl *);
int page_ftl_update_map(struct page_ftl *, size_t sector, uint32_t ppn);
struct device_address page_ftl_get_map(struct page_ftl *, size_t sector);

static inline size_t page_ftl_get_map_size(struct page_ftl *pgftl)
{
	struct device *dev = pgftl->dev;
	return ((device_get_total_size(dev) / device_get_page_size(dev)) + 1) *
	       sizeof(uint32_t);
}
static inline size_t page_ftl_get_lpn(struct page_ftl *pgftl, size_t sector)
{
	return sector / device_get_page_size(pgftl->dev);
}

static inline size_t page_ftl_get_page_offset(struct page_ftl *pgftl,
					      size_t sector)
{
	return sector % device_get_page_size(pgftl->dev);
}
#endif
