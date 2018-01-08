#include "cache.h"
/*
 * Implementing a double linked list cache.
 * New cache is added to front of the list.
 * When ever cache is used it is removed from list
 * and inserted at front.Therefore, the least 
 * reused ones which are at back are evicted from tail. 
 */


/*
 * function to initialize the cache which sets 
 * list head and tail to null and size to zero.
 * it returs a pointer to cache list.
 */
cache_list_t *cache_init()
{
  cache_list_t *list = Malloc(sizeof(cache_list_t));
  list->head = NULL;
  list->tail = NULL;
  list->cache_size = 0;
  return list;
}

/*
 * helper function to remove cache blocks from list
 */
void remove_from_list(cache_list_t *list, cache_block_t *block)
{  
   cache_block_t *prev_block = block->prev;
   cache_block_t *next_block = block->next;
  
   if ((prev_block == NULL) && (next_block == NULL)){
      list->head = NULL;
      list->tail = NULL;
   }
   else if((prev_block == NULL) && (next_block != NULL)){
      next_block->prev = NULL;
      list->head = next_block;
   }
   else if((prev_block != NULL) && (next_block == NULL)){
      prev_block->next = NULL;
      list->tail = prev_block;
   }
   else{
      prev_block->next = next_block;
      next_block->prev = prev_block;
   }
   list->cache_size = list->cache_size - block->size;
   
}

/*
 * helper function to update the cache block to the list.It adds 
 * blok to the begening of the linked list.
 */
void update_list(cache_list_t *list,cache_block_t *block)
{
     if (list->head == NULL){
         block->prev = NULL;
         block->next = NULL;
         list->head = block;
         list->tail = block;
      }
      else{
         block->prev = NULL;
         block->next = list->head;
         (list->head)->prev = block;
         list->head = block;
      }
      list->cache_size = list->cache_size + block->size;
  
} 
 
/*
 * helper function to create a new cache block of requested
 * payload size and  the necessary identifying information.
 */
cache_block_t *create(char *host, char *port, char *path,
                                char *payload ,size_t size) 
{ 
  cache_block_t *block = Malloc(sizeof(cache_block_t));
  block->host = Malloc(strlen(host)+1);
  strcpy(block->host,host);
  block->port = Malloc(strlen(port)+1);
  strcpy(block->port,port);
  block->path = Malloc(strlen(path)+1);
  strcpy(block->path,path);
  block->payload = Malloc(size);
  memcpy(block->payload,payload,size);
  block->size = size;
  return block;
}
  
/*
 * the helper function to find whether there is already an existing
 * cache block for the given request. Returs the pointer to block
 * on sucess or returs NULL if there is no such block
 */
cache_block_t *find(cache_list_t *list,char *host, char *port, char *path)
{
  cache_block_t *block;
  for(block = list->tail; block != NULL ; block = block->prev){
      if ( (!strcmp(block->host,host)) &&  (!strcmp(block->port,port)) &&
                                           (!strcmp(block->path,path))){ 
             return block;
      }
  }
  return NULL;
}

/*
 * helper function to evict a cache_block if the cache size is
 * above the maximum permissible limit.It uses the LRU policy to
 * evict by removing the block from tail.
 */
void evict(cache_list_t *list)
{
  cache_block_t *block;
  if (list->head == list->tail)
  {
     block = list->tail;
     list->head = NULL;
     list->tail = NULL;
  }    
  else      
  {
     block = list->tail;
     ((list->tail)->prev)->next = NULL;
     list->tail = (list->tail)->prev;
  }
  list->cache_size = list->cache_size - block->size;
  Free(block->host);
  Free(block->port);
  Free(block->path);
  Free(block->payload);
  Free(block);
}
