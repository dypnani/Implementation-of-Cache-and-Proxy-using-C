#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

typedef struct cache_block
{
     char *host;
     char *port;
     char *path;
     struct cache_block *prev;
     struct cache_block *next;
     char *payload;
     size_t size;

}cache_block_t;

typedef struct cache_list
{
     size_t cache_size;
     cache_block_t *head;
     cache_block_t *tail;
     
}cache_list_t;

cache_list_t *cache_init();
void update_list(cache_list_t *list, cache_block_t *block);
void remove_from_list(cache_list_t *list, cache_block_t *block);

cache_block_t *create(char *host, char *port, char *path, char *payload, size_t size);
cache_block_t *find(cache_list_t *list, char *host, char *port, char *path);
void evict(cache_list_t *list);

#endif
