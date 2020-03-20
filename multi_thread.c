#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "util.h"
#define MAX 1025
#define BUFFER_CAPACITY 1025

struct variables{
  pthread_mutex_t lock;
  pthread_cond_t condc, condp;
  FILE * output;
  FILE *serviced;
  char line[MAX];
  int files;
//  int producerThread;;
  int prod_thread_active;
  int producing;
  int producerThreads;
  int still_working;
  pthread_t consumer_thread;
};


struct variables vars;

typedef struct shared_buff
{
    void *buffer;     // data buffer
    void *buffer_end; // end of data buffer
    size_t capacity;  // maximum number of items in the buffer
    size_t count;     // number of items in the buffer
    size_t totalwrites;
    size_t totalreads;
    size_t sz;        // size of each item in the buffer
    void *head;       // pointer to head
    void *tail;       // pointer to tail
} shared_buff;

shared_buff sb;

void sb_init(shared_buff *sb, size_t capacity, size_t sz)
{
    sb->buffer = malloc(capacity * sz);
    if(sb->buffer == NULL)
        // handle error
    sb->buffer_end = (char *)sb->buffer + capacity * sz;
    sb->capacity = capacity;
    sb->count = 0;
    sb->totalreads = 0;
    sb->totalwrites = 0;
    sb->sz = sz;
    sb->head = sb->buffer;
    sb->tail = sb->buffer;
}

void sb_free(shared_buff *sb)
{
    free(sb->buffer);
    // clear out other fields too, just to be safe
}

void sb_push(shared_buff *sb, const void *item)
{
    if(sb->count == sb->capacity){
        printf("Error buffer full\n");
    }
    memcpy(sb->head, item, sb->sz);
    //if(sb->count != capacity)
    //might have to consider how to change when head incremented
    sb->head = (char*)sb->head + sb->sz;
    if((sb->head == sb->buffer_end) && (sb->tail != sb->buffer))
        sb->head = sb->buffer;
    sb->count++;
    sb->totalwrites++;
}

void *sb_pop(shared_buff *sb, void *item)
{
    if(sb->count == 0){
        printf("Error buffer empty, cant pop for empty buff\n");
    }
    if(sb->count > 0 && sb->head != sb->tail){
      memcpy(item, sb->tail, sb->sz);
      sb->tail = (char*)sb->tail + sb->sz;
      if((sb->tail == sb->buffer_end) && (sb->head != sb->buffer))
          sb->tail = sb->buffer;
      sb->count--;
      sb->totalreads++;
    }
    return item;
}

void * producer(char *input){
//  printf("In producer thread: %d\n", pthread_self());
  FILE * open = fopen(input, "r");
  //vars.still_working =1
  while (sb.count == MAX -1){
    printf("Buffer is full\n");
    pthread_cond_wait(&vars.condp, &vars.lock);
  }

	while(fscanf(open, "%s \n ", vars.line) !=EOF){
    pthread_mutex_lock(&vars.lock);
    char *host = strdup(vars.line);

      // make producers wait if the buffer is full
		sb_push(&sb, host);
    printf("%s: pushed to buffer \t, count: %d \n", host, sb.count);
    //printf("Pushed %s to buff \n", vars.line);      //makes sb.count = 1
    pthread_cond_signal(&vars.condc);
    //pthread_cond_signal(&vars.condc);
    pthread_mutex_unlock(&vars.lock);
	}

  fclose(open);
  pthread_cond_signal(&vars.condc);
  vars.prod_thread_active --;
  pthread_exit(0);
}
void * consumer(){
  printf("In consumer thread %d\n", pthread_self());

  char ipstr[INET6_ADDRSTRLEN];
  char* hostname;
  //char *hostname;
  while(sb.count == 0){
    printf("waiting on producer\n");
    pthread_mutex_lock(&vars.lock);
    pthread_cond_wait(&vars.condc, &vars.lock);
  }
  pthread_mutex_unlock(&vars.lock);
  while(vars.still_working == 1 || ((sb.head != sb.tail) && sb.count > 0) ){
  //  printf("sb count: %d\n", sb.count);
    pthread_mutex_lock(&vars.lock);
    hostname = sb_pop(&sb, vars.line);
    //printf("test: %s, hostname %s \n",test, hostname);
    //printf("Still working = %d \t count = %d \n", vars.still_working, sb.count);
    //printf("%d, Popped from stack %s \n", pthread_self(), hostname);
    pthread_mutex_unlock(&vars.lock);

    if(dnslookup(hostname, ipstr, sizeof(ipstr))== UTIL_FAILURE){
      fprintf(stderr,  "error looking up ipaddr %s\n", hostname);
      strncpy(ipstr, "", sizeof(ipstr));
    }

    fprintf(vars.output, "%d, %s, %s \n", pthread_self(), hostname, ipstr);
  }
//  pthread_mutex_unlock(&vars.lock);
  pthread_exit(0);
}

int main(int argc, char * argv[]){

  //gettimeofday(&start, NULL);
  vars.still_working =1;
  vars.files = 0;
  vars.prod_thread_active = 0;
  vars.producing =1;
  int inputFiles = argc -5;
  vars.producerThreads  = atoi(argv[(argc-inputFiles-3)]);
  int consumerThreads = atoi(argv[(argc-inputFiles-4)]);
  printf("consumer threads: %d \n", consumerThreads);
  printf("producer threads: %d \n", vars.producerThreads);
  pthread_t pro[vars.producerThreads], con[consumerThreads];
  //Initialize the shared buffer
  sb_init(&sb,BUFFER_CAPACITY,BUFFER_CAPACITY);
  pthread_cond_init(&vars.condc, NULL);		/* Initialize consumer condition variable */
  pthread_cond_init(&vars.condp, NULL);    // Initialize producer condition variable

  if(pthread_mutex_init(&vars.lock, NULL) != 0){
    printf("\n Mutex init failed \n");
    return 1;
  }
  vars.output = fopen(argv[(argc - inputFiles -2)], "w");
  vars.serviced= fopen(argv[argc - inputFiles -1], "w");

  int thred_count_cons=0;
  for(int i = 0; i < inputFiles; i++){

    //fprintf(serviced, "Consumer thread %d serviced file %s \n", producerThread, argv[i+5]);
    pthread_create(&(pro[i]), NULL, producer,(void*) argv[i+5]);
    vars.prod_thread_active ++;
    //printf("%d\n", thred_count_cons);
  }
  for(int i = 0; i < consumerThreads; i++){

    pthread_create(&(con[i]), NULL, consumer, NULL);
  }

  for(int i = 0; i < vars.producerThreads; i++){
    pthread_join(pro[i], NULL);
  }

  vars.still_working = 0;

  for(int i = 0; i < consumerThreads; i++){
    printf("consumer thread joing\n");
    pthread_join(con[i], NULL);
  }

    fclose(vars.output);
    //gettimeofday(&end, NULL);
  //  printf("time to complete: %ld nanoseconds \n", (((end.tv_sec*1000000 + end.tv_usec) - (start.tv_sec*1000000 + start.tv_usec))));
    sb_free(&sb);
    pthread_mutex_destroy(&vars.lock);
    pthread_cond_destroy(&vars.condc);		/* Free up consumer condition variable */
    pthread_cond_destroy(&vars.condp);		/* Free up producer condition variable */

    printf("sb totalwrites: %d \t sb totalreads: %d \n", sb.totalwrites, sb.totalreads);
    return 0;
}
