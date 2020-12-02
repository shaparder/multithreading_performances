#include <stdio.h>
#include <zconf.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#define WITR 640
#define RITR 2560

volatile int write_mutex;
volatile int read_mutex;
volatile int z_mutex;

typedef struct sem {
  volatile int nb;
  volatile int mutex;
} semaphore_t;

void lock_ts(volatile int *lock);
void unlock_ts(volatile int *lock);
void seminit(semaphore_t* new_sem, volatile int nb);
void semwait(semaphore_t* sem);
void sempost(semaphore_t* sem);

semaphore_t write_sem;
semaphore_t read_sem;

volatile int readcount = 0;
volatile int writecount = 0;
__thread int W_iter = 0;
__thread int R_iter = 0;

void *Writer(void *param) {

  int iter = *((int *) param);

  while(W_iter < iter)
  {
    lock_ts(&write_mutex);

    W_iter++;
    writecount++;
    if (writecount == 1) semwait(&read_sem);

    unlock_ts(&write_mutex);

    semwait(&write_sem);
    //écriture des données
    while(rand() > RAND_MAX/10000);
    sempost(&write_sem);

    lock_ts(&write_mutex);

    writecount--;
    if(writecount == 0) sempost(&read_sem);

    unlock_ts(&write_mutex);
  }

  free(param);
  return NULL;
}

void *Reader(void *param) {
  int iter = *((int *) param);

  while(R_iter < iter)
  {
    lock_ts(&z_mutex);  //un seul reader sur read_sem
    semwait(&read_sem);
    lock_ts(&read_mutex);

    R_iter++;
    readcount+=1;
    if (readcount == 1) semwait(&write_sem);

    unlock_ts(&read_mutex);

    sempost(&read_sem); //libération du prochain reader
    unlock_ts(&z_mutex);

    //lecture des données
    while(rand() > RAND_MAX/10000);
    lock_ts(&read_mutex);
    readcount-=1;
    if(readcount == 0) {
      sempost(&write_sem);
    }
    unlock_ts(&read_mutex);
  }
  free(param);
  return NULL;
}

int main(int argc, char *argv[]) {

  if (argc != 3) {
    perror("2 arguments required");
  }
  int n_write = strtol(argv[1], NULL, 10);
  int n_read = strtol(argv[2], NULL, 10);

  volatile int nb = 1;
  seminit(&write_sem, nb);
  seminit(&read_sem, nb);
  write_mutex = 0;
  read_mutex = 0;
  z_mutex = 0;

  pthread_t write_threads[n_write];
  pthread_t read_threads[n_read];
  int err;

  //création des threads writer
  for(int i=0; i < n_write; i++) {
    int *arg = (int *)malloc(sizeof(*arg));
    *arg = (i < n_write - 1) ? (WITR/n_write) : (WITR - (WITR/n_write)*i);
    err = pthread_create(& write_threads[i], NULL, Writer, arg);
    if (err != 0) {
      perror("pthread_create");
    }
  }

  //crétion des threads reader
  for(int i=0; i < n_read; i++) {
    int *arg = (int *)malloc(sizeof(*arg));
    *arg = (i < n_read - 1) ? (RITR/n_read) : (RITR - (RITR/n_read)*i);
    err=pthread_create(&read_threads[i], NULL, Reader, arg);
    if(err!=0) {
      perror("pthread_create");
    }
  }

  //join thread writer
  for(int i=0; i < n_write; i++) {
    err=pthread_join(write_threads[i], NULL);
    if(err!=0) {
      perror("pthread_create");
    }
  }

  //join thread reader
  for(int i=0; i < n_read; i++) {
    err=pthread_join(read_threads[i], NULL);
    if(err!=0) {
      perror("pthread_create");
    }
  }

  return 0;
}