#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define NUM_CORES (4) // you're supposed to use the get_nprocs function, but MacOS doesn't have that.

bool is_verbose = false;

// Helper functions
void print_verbose(char *fmt, ...) {
  if (is_verbose) {
    va_list args;
    va_start(args,fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}

void die_if(bool condition, char *fmt, ...) {
  if (condition) {
    va_list args;
    va_start(args,fmt);
    printf("Error: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
  }
}

void *malloc_or_die(size_t size, char *fmt, ...) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    va_list args;
    va_start(args,fmt);
    printf("Failed to malloc ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
  }
  return ptr;
}

// state struct and its procedures

// this is a struct of two arrays.
// if you use its special procedures, it will automatically resize the arrays
// in order to fit more compressed data.
// there is one of these per thread.
typedef struct state_t {
  size_t size;
  int index;
  int *num_chars_arr;
  char *chars_arr;
} state_t;

void state_resize(state_t *this, size_t size) {
  this->size = size;

  int *new_num_chars_arr = (int *) malloc_or_die(sizeof(int) * size, "num_chars_arr of size %i", size);
  memcpy(new_num_chars_arr, this->num_chars_arr, sizeof(int) * this->index);
  free(this->num_chars_arr);
  this->num_chars_arr = new_num_chars_arr;

  char *new_chars_arr = (char *) malloc_or_die(sizeof(char) * size, "chars_arr of size %i", size);
  memcpy(new_chars_arr, this->chars_arr, this->index);
  for (int i = 0; i < this->index; i++) {
    die_if(new_chars_arr[i] != this->chars_arr[i], "%c %c", new_chars_arr[i], this->chars_arr[i]);
  }
  free(this->chars_arr);
  this->chars_arr = new_chars_arr;
}

void state_init(state_t *this) {
  int size = 128;
  this->index = 0;
  this->size = size;
  this->num_chars_arr = (int *) malloc_or_die(sizeof(int) * size, "num_chars_arr of size %i", size);
  this->chars_arr = (char *) malloc_or_die(sizeof(char) * size, "chars_arr of size %i", size);  
}

// if you try to add to it when it's full, it doubles in size
void state_add(state_t *this, int num_chars, char c) {
  if (this->index == this->size) {
    state_resize(this, this->size * 2);
  }
  this->num_chars_arr[this->index] = num_chars;
  this->chars_arr[this->index] = c;
  this->index++;
}

void state_merge(state_t *dst, state_t *src) {
  if (src->index == 0) {
    return; // just in case this gets called with an empty state_t
  }
  int i;
  if (dst->chars_arr[dst->index-1] == src->chars_arr[0]) {
    dst->num_chars_arr[dst->index-1] += src->num_chars_arr[0];
    i = 1;
  } else {
    i = 0;
  }
  for (; i < src->index; i++) {
    state_add(dst, src->num_chars_arr[i], src->chars_arr[i]);
  }
}

void state_free(state_t *this) {
  free(this->num_chars_arr);
  free(this->chars_arr);
}

void state_print(state_t *this) {
  printf("index: %i ", this->index);
  for (int i = 0; i < this->index; i++) {
    if (this->chars_arr[i] == '\n') {
      printf("(%i, %s) ", this->num_chars_arr[i], "\\n");
    } else {
      printf("(%i, %c) ", this->num_chars_arr[i], this->chars_arr[i]);
    }
  }
  printf("\n");
}

// stuff for the threads
typedef struct thread_args_t {
  int start_index;
  int end_index;
  char *text;
  state_t state;
} thread_args_t;

// compresses 1 / NUM_CORES of the file
void *thread_func(void *threadArgsVoid) {
  thread_args_t *thread_args = (thread_args_t *) threadArgsVoid;
  char *text = thread_args->text;

  int i = thread_args->start_index + 1;
  int num_chars = 1;
  char curr_char = text[thread_args->start_index];
  while(i < thread_args->end_index) {
    if (text[i] != curr_char) {
      state_add(&(thread_args->state), num_chars, curr_char);
      num_chars = 1;
      curr_char = text[i];
    } else {
      num_chars++;
    }
    i++;
  }
  state_add(&(thread_args->state), num_chars, curr_char);
  return NULL;
}


int main(int argc, char **argv) {
  if (argc < 2) {
    if (argv[0][0] == '.' && argv[0][1] == '/') {
      printf("%s: file1 [file2 ...]\n", argv[0] + 2);
    } else {
      printf("%s: file1 [file2 ...]\n", argv[0]);
    }
    exit(1);
  }
  
  state_t combined_state;
  state_init(&combined_state);
  // iterate over each filepath in argv
  for (int i = 1; i < argc; i++) {
    char *filepath = argv[i];

    int fd = open(filepath, O_RDONLY);
    die_if(fd < 0, "could not open %s", filepath);

    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    die_if(err < 0, "could not get stats on %s", filepath);

    char *text = mmap(NULL,statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    die_if(text == MAP_FAILED, "mmap failed on %s", filepath);
    close(fd);

    int bytes_per_thread = statbuf.st_size / NUM_CORES;

    // initialize state for threads
    thread_args_t *thread_args = (thread_args_t *) malloc_or_die(sizeof(thread_args_t) * NUM_CORES, "thread_args");
    for (int i = 0; i < NUM_CORES; i++) {
      thread_args[i].start_index = i * bytes_per_thread;
      thread_args[i].end_index = (i+1) * bytes_per_thread;
      thread_args[i].text = text;
      state_init(&(thread_args[i].state));
    }
    thread_args[NUM_CORES - 1].end_index = statbuf.st_size;

    print_verbose("File size: %lli\n\n", statbuf.st_size);
    if (is_verbose) {
      for (int i = 0; i < NUM_CORES; i++) {
        print_verbose("thread %i: (%i, %i)\n", i, thread_args[i].start_index , thread_args[i].end_index);
      }
    }


    // run threads on sub-portions of the file
    pthread_t *threads = (pthread_t *) malloc_or_die(sizeof(pthread_t) * NUM_CORES, "threads");
    for (int i = 0; i < NUM_CORES; i++) {
      int rc = pthread_create(&threads[i], NULL, &thread_func, (void *) &thread_args[i]);
      die_if(rc != 0, "Error creating thread %i", i);
    }
      
    // join the threads
    print_verbose("\n");
    for (int i = 0; i < NUM_CORES; i++) {
      int thread_rc = pthread_join(threads[i], NULL);
      die_if(thread_rc != 0, "Error joining thread %i", i);
      if (is_verbose) {
        state_print(&(thread_args[i].state));
      }
    }
      
    // merge the outputs
    for (int i = 0; i < NUM_CORES; i++) {
      state_merge(&(combined_state), &(thread_args[i].state));
    }
    if (is_verbose) {
      printf("\n");
      state_print(&combined_state);
    }

    // cleanup
    err = munmap(text, statbuf.st_size);
    die_if(err != 0, "munmap failed on %s", filepath);
    free(threads);
    for (int i = 0; i < NUM_CORES; i++) {
      state_free(&(thread_args[i].state));
    }
    free(thread_args);
  }

  // print the outputs to standard out
  for (int i = 0; i < combined_state.index; i++) {
    fwrite(&(combined_state.num_chars_arr[i]), 4, 1, stdout);
    printf("%c", combined_state.chars_arr[i]);
  }

  // final_cleanup
  state_free(&combined_state);

  return 0;
}
