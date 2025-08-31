#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>

#define QUEUESIZE 10
#define LOOP 1000


double s_time;


void *producer (void *args);
void *consumer (void *args);

void *routine (void *args){
  long int  i = 10^100; // Some dummy operation.

}

typedef struct {
  void * (*work)(void *);
  void * arg;
} workFunction;

typedef struct {
  workFunction buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction pi);
void queueDel (queue *q, workFunction *out);

int main (int argc, char* argv[])
{
  char sp[100], sq[100];
  char c1[]="_";
  char c2[]=".txt";

  int i, j, pn, qn;
  pn = atoi (argv[1]);
  qn = atoi (argv[2]);
  
  sprintf(sp, "%d", pn);    //This is where we construct the string that will be the name of the result's file.
  sprintf(sq, "%d", qn);
  strcat(sp,c1);
  strcat(sp,sq);
  strcat(sp,c2);
  printf("this is the name of the result's file %s\n", sp);
  
  int fp = open(sp, O_WRONLY|O_CREAT, 0777);    //Opening the file in which we we will store the results.
  dup2(fp, STDOUT_FILENO);    // Making the sdout of the current program as the input for the file that will hold the results.
  
  
  pthread_t *pro;
  pthread_t *con;
  pro = (pthread_t *)malloc(pn*sizeof(pthread_t));
  con = (pthread_t *)malloc(qn*sizeof(pthread_t));

  
  queue *fifo;
  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }
  for(i=0;i<pn;i++){
    if (pthread_create (&pro[i], NULL, producer, fifo)!=0){
      printf("Failed to create thread!");
      return 1;
    }
  }
  
  for(j = 0; j < qn; j++){
    if (pthread_create (&con[j], NULL, consumer, fifo)!=0){
      printf("Failed to create thread!");
      return 1;
    }
  }
  for(i = 0; i < pn; i++){
    if (pthread_join (pro[i], NULL)!=0){
      printf("Failed to join producer's threads!");
      return 1;
    };
  }

  while(fifo->empty==0){
    //wait till the queue is empty

  }
  
  for(i=0;i<qn;i++){
    pthread_cancel(con[i]);
  }

  free(pro);
  free(con);
  close(fp);
  return 0;
}

void *producer (void *q)
{
  int i;
  queue *fifo;
  fifo = (queue *)q;
 

  for(i=0; i<LOOP; i++){

    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      //printf ("producer: queue FULL.\n");
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    workFunction pi;

    struct timeval * startwtime;
    startwtime = (struct timeval *) malloc(sizeof(struct timeval));

    gettimeofday (startwtime, NULL);
  
    pi.work = routine;
    pi.arg = startwtime;
    queueAdd (fifo, pi);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
  }
  return(NULL);
}

void *consumer (void *q)
{
  queue *fifo;

  int i;
  while(1){
    fifo = (queue *)q;
    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      //printf ("consumer: queue EMPTY.\n");
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    } 
    workFunction out;
    struct timeval endwtime;
    queueDel (fifo, &out);
    struct timeval *t = out.arg;

    gettimeofday (&endwtime, NULL);
    s_time = (double)((endwtime.tv_usec -  t->tv_usec)/1.0e6 + endwtime.tv_sec - t->tv_sec);
    printf("%lf\n", s_time);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);
    out.work(NULL);
    free(out.arg);
    
  }
  return(NULL);
}

queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, workFunction pi)
{
  q->buf[q->tail] = pi;
  
  q->tail++;

  if (q->tail == QUEUESIZE){
    q->tail = 0;
  }

  if (q->tail == q->head){
    q->full = 1;
  }
  q->empty = 0;
  return;

}

void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];
  q->head++;
  if (q->head == QUEUESIZE){
    q->head = 0;
  }
  if (q->head == q->tail){
    q->empty = 1;
  }
  q->full = 0;

  return;
}