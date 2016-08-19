
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "time_meas.h"

#include "ring_buffer.h"


int
alt_init_rb(st_ring_buf *rb, int size)
{
	int res;
	
	rb->size = size + 1;
	rb->start = 0;
	rb->end = 0;
	rb->elems =(st_elems *)malloc(rb->size * SIZE_ELEMS);

	rb->writemutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));

	res = pthread_mutex_init(rb->writemutex, NULL);

	rb->readmutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	res = pthread_mutex_init(rb->readmutex, NULL);
	return res;
}

int
alt_rb_full(st_ring_buf *rb)
{
	return ((rb->end + 1 ) % rb->size) == rb->start;
}

int
alt_rb_empty(st_ring_buf *rb)
{
	return rb->end == rb->start;
}

int
alt_rb_write(st_ring_buf *rb, st_elems *e,int set_sn)
{
 
 	int res;
	res = pthread_mutex_lock(rb->writemutex);
 	
	if(alt_rb_full(rb))
	{
		res = pthread_mutex_unlock(rb->writemutex);
		return WRITE_ERR;
	}

        if(set_sn)
		e->sn=e->dltatime.in;

	memcpy(rb->elems+(rb->end),e,SIZE_ELEMS);
	rb->end = (rb->end + 1) % rb->size;
	res = pthread_mutex_unlock(rb->writemutex);
	if(res!=0) return res;
	return WRITE_FINISH;
}

int
alt_rb_read(st_ring_buf *rb, st_elems *e)
{
	pthread_mutex_lock(rb->readmutex);

	if(alt_rb_empty(rb))
	{
		pthread_mutex_unlock(rb->readmutex);
		return READ_ERR;
	}
	memcpy(e,rb->elems+(rb->start),SIZE_ELEMS);	
	rb->start = (rb->start + 1) % rb->size;
	

	pthread_mutex_unlock(rb->readmutex);
	
	return READ_FINISH;
}

void
alt_del_rb(st_ring_buf* rb)
{
	pthread_mutex_destroy(rb->readmutex);
	pthread_mutex_destroy(rb->writemutex);
	free(rb->elems);
	free(rb);

}
