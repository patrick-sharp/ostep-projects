#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "mapreduce.h"

/*
  
  How MapReduce works:

  1. Mapping phase
  Create num_mappers threads.
  Divide work among the threads so that `map` will be called on all elements of argv
  `map` will call `MR_Emit` on all keys/values that need to be reduced.
  `MR_Emit` can be called multiple times with the same key and value - that just means
  an extra copy of that key/value pair should be stored in the key/value pair list.
  My implentation of the key/value pair list is a dynamic array of KeyAndValues structs where each struct
  contains a key and a dynamic array of values.
  When `MR_Emit` is called, it looks for the key. If it finds it, it adds the value to the list.
  If it doesn't, it adds a new struct with that key and one member of its list (the value).

  2. Sorting phase
  Sort the outer array of KeyAndValues structs by key.
  Sort each array of values.

  3. Reducing phase
  Reduce is called once per unique key.
  All key/value pairs with the same key are reduced on the same thread.
  Create num_reducers threads.
  Divide work among the threads so that `reduce` is called once on all elements of the dynamic
  array of KeyAndValues structs.
  `reduce` will call `get_next` to get all the values for a given key until it runs out.
  `get_next` just traverses the dynamic arry in each KeyAndValues struct and returns values.

*/

#define DEFAULT_DYN_ARR_CAPACITY (128)



typedef struct KeyAndValues {
  char *key;
  char **values;
  int size;
  int capacity;
} KeyAndValues;

typedef struct KeyValueStore {
  KeyAndValues *key_values_arr;
  int size;
  int capacity;
} KeyValueStore;

KeyValueStore global_kv_store;

void init_kv_store() {
  global_kv_store.key_values_arr = (KeyAndValues *) malloc(DEFAULT_DYN_ARR_CAPACITY * sizeof(KeyAndValues));
  assert(global_kv_store.key_values_arr != NULL);
  global_kv_store.size = 0;
  global_kv_store.capacity = DEFAULT_DYN_ARR_CAPACITY;
}

void free_kv_store() {
  free(global_kv_store.key_values_arr);
}

void MR_Emit(char *key, char *value) {

}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

typedef struct MapThreadArgs {
  Mapper map;
  char** argv;
  int start_index;
  int end_index;
} MapThreadArgs;

void *map_thread_func(void *map_thread_args_void) {
  MapThreadArgs *args_p = (MapThreadArgs *) map_thread_args_void; 
  printf("map_thread_args %i %i\n", args_p->start_index, args_p->end_index);
  return NULL;
}

void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition) {
  // initialize global kv_store
  init_kv_store();

  // create mapper threads
  pthread_t *mappers = (pthread_t *) malloc(num_mappers * sizeof(pthread_t));
  MapThreadArgs *map_thread_args_arr = (MapThreadArgs *) malloc(num_mappers * sizeof(MapThreadArgs));
  for (int i = 0; i < num_mappers; i++) {
    map_thread_args_arr[i].map = map;
    map_thread_args_arr[i].argv = argv;
    map_thread_args_arr[i].start_index = 1 + i * ((argc - 1) / num_mappers);
    if (i == num_mappers - 1) {
      map_thread_args_arr[i].end_index = argc - 1;
    } else {
      map_thread_args_arr[i].end_index = 1 + (i + 1) * ((argc - 1) / num_mappers);
    }
    assert(pthread_create(&(mappers[i]), NULL, map_thread_func, &(map_thread_args_arr[i])) == 0);
  }

  // join mapper threads
  for (int i = 0; i < num_mappers; i++) {
    assert(pthread_join(mappers[i], NULL) == 0);
  }

  // cleanup mappers
  free(mappers);
  free(map_thread_args_arr);

  // sort kv_store

  // create reducer threads
  
  // join reducer threads


  pthread_t *reducers = (pthread_t *) malloc(num_reducers * sizeof(pthread_t));

  free(reducers);
  free_kv_store();
}

