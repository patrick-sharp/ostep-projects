#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapreduce.h"

/*
  
  How MapReduce works:

  1. Mapping phase
  Create num_mappers threads.
  Divide work among the threads so that `map` will be called on all elements of argv
  `map` will call `MR_Emit` on all keys/values that need to be reduced.
  `MR_Emit` can be called multiple times with the same key and value - that just means
  an extra copy of that key/value pair should be stored in the key/value pair list.
  The number of reducers is the number of partitions. Since each key has a partition, map
  can add any key to any partition because you never know what key map will call MR_Emit with.
  My implentation of the key/value pair list is a an array of num_reducers KVStore structs.
  KVStore structs are basically lockable dynamic arrays of KeyAndValues structs. KeyAndValues structs
  have a key string and a dynamic arrays of value strings.
  When `MR_Emit` is called, it looks for the key. If it finds it, it adds the value to the list.
  If it doesn't, it adds a new struct with that key and one member of its list (the value).
  Each KVStore struct must be lockable so that you don't get the same key added twice or a key
  overriding the position of a previous key.

  2. Sorting phase
  Sort the outer array of KeyAndValues structs by key.
  Sort each array of values alphabetically.

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

bool is_verbose = false;

typedef struct KeyAndValues {
  char *key;
  char **values;
  int size;
  int capacity;
  int index; // Used for get_next in the reducing phase.
} KeyAndValues;

typedef struct KVStore {
  KeyAndValues *key_values_arr;
  int size;
  int capacity;
  pthread_mutex_t mutex;
} KVStore;

KVStore *stores;
Mapper global_map;
Reducer global_reduce;
Partitioner global_partition;
int num_partitions;

void print_kv_keys(int partition_num) {
  if (is_verbose) {
    KVStore *kvs_p = &(stores[partition_num]);
    printf("KV store keys:");
    for (int i = 0; i < kvs_p->size; i++) {
      printf(" %s", kvs_p->key_values_arr[i].key);
    }
    printf("\n");
  }
}

void print_kv_state(int partition_num) {
  if (is_verbose) {
    KVStore *kvs_p = &(stores[partition_num]);
    printf("KV store state %i %i:\n", kvs_p->size, kvs_p->capacity);
    for (int i = 0; i < kvs_p->size; i++) {
      KeyAndValues *kav_p = &(kvs_p->key_values_arr[i]);
      printf("%s %i %i:", kav_p->key, kav_p->size, kav_p->capacity);
      for (int j = 0; j < kav_p->size; j++) {
        printf(" %s", kav_p->values[j]);
      }
      printf("\n");
    }
  }
}

void print_stores_state() {
  for (int i = 0; i < num_partitions; i++) {
    print_kv_keys(i);
  }
}

int compare_by_key(const void *a, const void *b) {
  KeyAndValues *kva_p = (KeyAndValues *) a;
  KeyAndValues *kvb_p = (KeyAndValues *) b;
  return strcmp(kva_p->key, kvb_p->key);
}

int qsort_strcmp(const void* a, const void* b) {
    const char* aa = *(const char**) a;
    const char* bb = *(const char**) b;
    return strcmp(aa,bb);
}

void init_stores() {
  stores = (KVStore *) malloc(num_partitions * sizeof(KVStore));
  assert(stores != NULL);
  for (int i = 0; i < num_partitions; i++) {
    KVStore *kvs_p = &(stores[i]);
    kvs_p->key_values_arr = (KeyAndValues *) malloc(DEFAULT_DYN_ARR_CAPACITY * sizeof(KeyAndValues));
    assert(kvs_p->key_values_arr != NULL);
    kvs_p->size = 0;
    kvs_p->capacity = DEFAULT_DYN_ARR_CAPACITY;
    pthread_mutex_init(&(kvs_p->mutex), NULL);
  }
}

void free_stores() {
  for (int i = 0; i < num_partitions; i++) {
    KVStore *kvs_p = &(stores[i]);
    for (int j = 0; j < kvs_p->size; j++) {
      KeyAndValues *kav_p = &(kvs_p->key_values_arr[j]);
      free(kav_p->key);
      for (int k = 0; k < kav_p->size; k++) {
        free(kav_p->values[k]);
      }
      free(kav_p->values);
    }
    free(kvs_p->key_values_arr);
  }
  free(stores);
}

void MR_Emit(char *key, char *value) {
  int partition_num = global_partition(key, num_partitions);
  KVStore *kvs_p = &(stores[partition_num]);

  pthread_mutex_lock(&(kvs_p->mutex));
  int key_index = -1;
  for (int i = 0; i < kvs_p->size; i++) {
    if (strcmp(key, kvs_p->key_values_arr[i].key) == 0) {
      key_index = i;
      break;
    }
  }

  if (key_index == -1) {
    // add a new KeyAndValues struct for this key
    if (kvs_p->size == kvs_p->capacity) {
      kvs_p->key_values_arr = (KeyAndValues *) realloc(kvs_p->key_values_arr, kvs_p->capacity * 2 * sizeof(KeyAndValues));
      assert(kvs_p->key_values_arr != NULL);
      kvs_p->capacity *= 2;
    }
    KeyAndValues *kav_p = &(kvs_p->key_values_arr[kvs_p->size]);
    kav_p->key = strdup(key);
    assert(kav_p->key != NULL);
    kav_p->values = (char **) malloc(DEFAULT_DYN_ARR_CAPACITY * sizeof(char *)),
    assert(kav_p->values != NULL);
    kav_p->values[0] = strdup(value);
    assert(kav_p->values[0] != NULL);
    kav_p->size = 1;
    kav_p->capacity = DEFAULT_DYN_ARR_CAPACITY;
    kav_p->index = 0;
    kvs_p->size++;
  } else {
    // add this value to an existing KeyAndValues struct's values array
    KeyAndValues *kav_p = &(kvs_p->key_values_arr[key_index]);
    if (kav_p->size == kav_p->capacity) {
      kav_p->values = (char **) realloc(kav_p->values, kav_p->capacity * 2 * sizeof(char *));
      assert(kav_p->values != NULL);
      kav_p->capacity *= 2;
    }
    kav_p->values[kav_p->size] = strdup(value);
    kav_p->size++;
  }
  pthread_mutex_unlock(&(kvs_p->mutex));
}

// no need for locking here since each key is only used by one reducing thread
char *get_next(char *key, int partition_number) {
  KVStore *kvs_p = &(stores[partition_number]);
  KeyAndValues *kav_p = NULL;
  for (int i = 0; i < kvs_p->size; i++) {
    if (strcmp(key, kvs_p->key_values_arr[i].key) == 0) {
      kav_p = &(kvs_p->key_values_arr[i]);
    }
  }
  assert(kav_p != NULL);
  if (kav_p->index == kav_p->size) {
    return NULL;
  }
  return kav_p->values[kav_p->index++];
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

typedef struct MapThreadArgs {
  char** argv;
  int start_index;
  int end_index;
} MapThreadArgs;

void *map_thread_func(void *map_thread_args_void) {
  MapThreadArgs *args_p = (MapThreadArgs *) map_thread_args_void; 
  if (is_verbose) {
    printf("map_thread_args %i %i\n", args_p->start_index, args_p->end_index);
  }
  for (int i = args_p->start_index; i < args_p->end_index; i++) {
    global_map(args_p->argv[i]);
  }
  return NULL;
}

void *reduce_thread_func(void *partition_num_void) {
  int *partition_num_p = (int *) partition_num_void;
  KVStore *kvs_p = &(stores[*partition_num_p]);
  for (int i = 0; i < kvs_p->size; i++) {
    global_reduce(kvs_p->key_values_arr[i].key, get_next, *partition_num_p);
  }
  return NULL;
}

void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition) {
  // initialize global state
  global_map = map;
  global_reduce = reduce;
  global_partition = partition;
  num_partitions = num_reducers;
  init_stores();

  // Create mapper threads
  if (is_verbose) {
    printf("Creating mapper threads\n");
  }
  pthread_t *mappers = (pthread_t *) malloc(num_mappers * sizeof(pthread_t));
  assert(mappers != NULL);
  MapThreadArgs *map_thread_args_arr = (MapThreadArgs *) malloc(num_mappers * sizeof(MapThreadArgs));
  // figure out how many elements of argv each thread mapper thread will handle
  int total = 0;
  int *num_args_arr = (int *) calloc(num_mappers, sizeof(int));
  assert(num_args_arr != NULL);
  while (total < argc - 1) {
    num_args_arr[total % num_mappers]++;
    total++;
  }
  // initialize thread args
  for (int i = 0; i < num_mappers; i++) {
    if (i == 0) {
      map_thread_args_arr[i].start_index = 1;
    } else {
      map_thread_args_arr[i].start_index = map_thread_args_arr[i-1].end_index;
    }
    map_thread_args_arr[i].end_index = map_thread_args_arr[i].start_index + num_args_arr[i];
    map_thread_args_arr[i].argv = argv;
    
    assert(pthread_create(&(mappers[i]), NULL, map_thread_func, &(map_thread_args_arr[i])) == 0);
  }
  free(num_args_arr);

  // Join mapper threads
  if (is_verbose) {
    printf("Joining mapper threads\n");
  }
  for (int i = 0; i < num_mappers; i++) {
    assert(pthread_join(mappers[i], NULL) == 0);
  }
  if (is_verbose) {
    printf("\n");
  }

  // Cleanup mappers
  free(mappers);
  free(map_thread_args_arr);

  // Sort kv_stores
  if (is_verbose) {
    printf("Sorting keys\n");
  }
  for (int i = 0; i < num_partitions; i++) {
    KVStore *kvs_p = &(stores[i]);
    qsort(kvs_p->key_values_arr, kvs_p->size, sizeof(KeyAndValues), &compare_by_key);
    for (int i = 0; i < kvs_p->size; i++) {
      KeyAndValues *kav_p = &(kvs_p->key_values_arr[i]);
      qsort(kav_p->values, kav_p->size, sizeof(char *), qsort_strcmp);
    }
  }
  print_stores_state();

  // Create reducer threads
  if (is_verbose) {
    printf("Creating reducer threads\n");
  }
  pthread_t *reducers = (pthread_t *) malloc(num_reducers * sizeof(pthread_t));
  assert(reducers != NULL);
  int *reduce_thread_args_arr = (int *) malloc(num_reducers * sizeof(int));
  for (int i = 0; i < num_reducers; i++) {
    reduce_thread_args_arr[i] = i;
    assert(pthread_create(&(reducers[i]), NULL, reduce_thread_func, &(reduce_thread_args_arr[i])) == 0);
  }

  // Join reducer threads
  if (is_verbose) {
    printf("Joining reducer threads\n");
  }
  for (int i = 0; i < num_reducers; i++) {
    assert(pthread_join(reducers[i], NULL) == 0);
  }
  free(reduce_thread_args_arr);
  free(reducers);
  free_stores();
}

