/**
 * @file lru.c
 * @brief implementation of the lru cache
 * @author Gijun Oh
 * @version 1.0
 * @date 2021-09-30
 */

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "include/log.h"
#include "include/lru.h"

struct lru_cache *lru_init(const size_t capacity, lru_dealloc_fn deallocate)
{
	struct lru_cache *cache = NULL;
	if (capacity == 0) {
		pr_err("capacity is zero\n");
		goto exception;
	}

	cache = (struct lru_cache *)malloc(sizeof(struct lru_cache));
	if (cache == NULL) {
		pr_err("memory allocation failed\n");
		goto exception;
	}

	cache->head = &cache->nil;
	cache->deallocate = deallocate;
	cache->capacity = capacity;
	cache->size = 0;

	cache->nil.next = &cache->nil;
	cache->nil.prev = &cache->nil;
	cache->nil.key = -1;
	cache->nil.value = (uintptr_t)NULL;

	return cache;
exception:
	lru_free(cache);
	return NULL;
}

static struct lru_node *lru_alloc_node(const uint64_t key, uintptr_t value)
{
	struct lru_node *node = NULL;
	node = (struct lru_node *)malloc(sizeof(struct lru_node));
	if (node == NULL) {
		pr_err("node allocation failed\n");
		return NULL;
	}
	node->key = key;
	node->value = value;
	return node;
}

static void lru_dealloc_node(struct lru_node *node)
{
	assert(NULL != node);
#if 0
	pr_debug("deallcate the node (key: %ld, value: %ld)\n", node->key,
		 node->value); /**< Not recommend to print this line */
#endif
	free(node);
}

static int lru_delete_node(struct lru_node *head, struct lru_node *deleted)
{
	assert(NULL != head);
	assert(NULL != deleted);

	if (head == deleted) {
		pr_debug(
			"deletion of head node is not permitted (node:%p, deleted:%p)\n",
			head, deleted);
		return -EINVAL;
	}
	deleted->prev->next = deleted->next;
	deleted->next->prev = deleted->prev;
	return 0;
}

static int __lru_do_evict(struct lru_cache *cache)
{
	struct lru_node *head = cache->head;
	struct lru_node *target = head->prev;
	int ret = 0;
	lru_delete_node(head, target);
	if (cache->deallocate) {
		ret = cache->deallocate(target->key, target->value);
	}
	lru_dealloc_node(target);
	return ret;
}

static int lru_do_evict(struct lru_cache *cache, const uint64_t nr_evict)
{
	int ret = 0;
	uint64_t i;
	for (i = 0; i < nr_evict; i++) {
		ret = __lru_do_evict(cache);
		if (ret) {
			return ret;
		}
		cache->size -= 1;
	}
	return ret;
}

static void lru_node_insert(struct lru_node *node, struct lru_node *newnode)
{
	assert(NULL != newnode);
	assert(NULL != node);

	newnode->prev = node;
	newnode->next = node->next;
	node->next->prev = newnode;
	node->next = newnode;
}

int lru_put(struct lru_cache *cache, const uint64_t key, uintptr_t value)
{
	struct lru_node *head = cache->head;
	struct lru_node *node = NULL;
	assert(NULL != head);

	if (cache->size >= cache->capacity) {
		pr_debug("eviction is called (size: %ld, cap: %ld)\n",
			 cache->size, cache->capacity);
		lru_do_evict(cache, lru_get_evict_size(cache));
	}

	node = lru_alloc_node(key, value);
	if (node == NULL) {
		pr_err("memory allocation failed\n");
		return -ENOMEM;
	}
	lru_node_insert(head, node);
	cache->size += 1;
	return 0;
}

static struct lru_node *lru_find_node(struct lru_cache *cache,
				      const uint64_t key)
{
	struct lru_node *head = cache->head;
	struct lru_node *it = head->next;
	while (it != head) {
		if (it->key == key) {
			return it;
		}
		it = it->next;
	}
	return NULL;
}

uintptr_t lru_get(struct lru_cache *cache, const uint64_t key)
{
	struct lru_node *head = cache->head;
	struct lru_node *node = NULL;
	uintptr_t value = (uintptr_t)NULL;
	node = lru_find_node(cache, key);
	if (node) {
		assert(0 == lru_delete_node(head, node));
		lru_node_insert(head, node);
		value = node->value;
	}
	return value;
}

int lru_free(struct lru_cache *cache)
{
	int ret = 0;
	struct lru_node *head = NULL;
	struct lru_node *node = NULL;
	struct lru_node *next = NULL;
	if (!cache) {
		return ret;
	}
	head = cache->head;
	node = head->next;
	while (node != head) {
		assert(NULL != node);
		next = node->next;
		if (cache->deallocate) {
			ret = cache->deallocate(node->key, node->value);
			if (ret) {
				pr_err("deallocate failed (key: %ld, value: %ld)\n",
				       node->key, node->value);
				return ret;
			}
		}
		free(node);
		node = next;
	}
	free(cache);
	return ret;
}
