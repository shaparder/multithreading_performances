#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

//define buffer size and number of iterations prod-cons
#define NBUF 8
#define NITR 1024

typedef struct primitive_sem
{
  int val;
  volatile int* lock;
} prim_sem;

typedef struct buffer
{
        int buf[NBUF];           /* shared var */
        int    in;               /* buf[in%BUFF_SIZE] is the first empty slot */
        int    out;              /* buf[out%BUFF_SIZE] is the first full slot */
        prim_sem*  sem_full;     /* keep track of the number of full spots */
        prim_sem*  sem_empty;    /* keep track of the number of empty spots */
        volatile int*  mutex;    /* enforce mutual exclusion to shared data */
}buf_t;

buf_t buf;

void lock_ts(volatile int *lock);
void unlock_ts(volatile int *lock);
int prim_sem_init(prim_sem **s, int start_val);
int prim_sem_destroy(prim_sem *sem);
int prim_sem_wait(prim_sem *sem);
int prim_sem_post(prim_sem *sem);


//check if char[] is number
bool isNumber(const char number[])
{
    int i = 0;

    //checking for negative numbers
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++)
    {
        //if (number[i] > '9' || number[i] < '0')
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}

//check command line arguments
void args_check(int argc, const char *argv[])
{
  if (argc != 3 || !isNumber(argv[1]) || !isNumber(argv[2]))
  {
    printf("wrong arguments\n");
    printf("usage: ./prodcons N_PRODUCERS N_CONSUMERS\n");
    exit(1);
  }
  return ;
}

int random_number()
{
  struct timeval tp;
  gettimeofday(&tp, NULL);

  srand(tp.tv_usec + getpid());
  int result = rand();
  return result;
}

void *Producer(void *param)
{
  int iter = *((int *) param);
  int to_store;
  int P_iter = 0;

  while (P_iter < iter)
  {
    while(rand() > RAND_MAX/10000);
    //increment iter count
    P_iter++;
    //compute random number
    to_store = random_number();
    //wait for empty spot in buffer
    prim_sem_wait(buf.sem_empty);
    //if another thread uses the buffer, wait
    lock_ts(buf.mutex);
    //store item
    buf.buf[buf.in] = to_store;
    buf.in = (buf.in+1)%NBUF;
    //release the buffer
    unlock_ts(buf.mutex);
    //increment the number of full slots
    prim_sem_post(buf.sem_full);
  }
  free(param);
  //printf("P_iter=%d\n", P_iter);
  return NULL;
}

void *Consumer(void *param)
{
  int iter = *((int *) param);
  int item;
  int C_iter = 0;

  while (C_iter < iter)
  {
    while(rand() > RAND_MAX/10000);

    prim_sem_wait(buf.sem_full);
    lock_ts(buf.mutex);
    //increment iter
    C_iter++;
    //get line
    item = buf.buf[buf.out];
    buf.out = (buf.out+1)%NBUF;
    //release buffer
    unlock_ts(buf.mutex);
    prim_sem_post(buf.sem_empty);
    //use the variable
    //printf("%d\n", item);
    (void)item;
  }
  free(param);
  //printf("C_iter=%d\n", C_iter);
  return NULL;
}


int main(int argc, const char* argv[])
{

  //security check for args
  args_check(argc, argv);

  //get threads numbers from command line
  int nprod = atoi(argv[1]);
  int ncons = atoi(argv[2]);

  //create threads array
  pthread_t prod_threads[nprod];
  pthread_t cons_threads[ncons];

  //init semaphore and mutex
  prim_sem_init(&(buf.sem_full), 0);
  prim_sem_init(&(buf.sem_empty), NBUF);
  buf.mutex = 0;

  //create all threads, handle number of iterations by computing it as arg
  for (int i = 0; i < nprod; i++)
  {
    int *arg = (int *)malloc(sizeof(*arg));
    *arg = (i < nprod - 1) ? (NITR/nprod) : (NITR - (NITR/nprod)*i);
    pthread_create(&prod_threads[i], NULL, Producer, arg);
  }
  for (int j = 0; j < ncons; j++)
  {
    int *arg = (int *)malloc(sizeof(*arg));
    *arg = (j < ncons - 1) ? (NITR/ncons) : (NITR - (NITR/ncons)*j);
    pthread_create(&cons_threads[j], NULL, Consumer, arg);
  }

  // join all threads
  void *ret = NULL;
  for (int i = 0; i < nprod; i++) pthread_join(prod_threads[i], &ret);
  for (int j = 0; j < ncons; j++) pthread_join(cons_threads[j], &ret);

  //exit semaphores
  prim_sem_destroy(buf.sem_full);
  prim_sem_destroy(buf.sem_empty);

  return 0;
}
