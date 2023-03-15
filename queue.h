/*******************************************************************************
 * FILENAME: queue.h
 *
 * DESCRIPTION:
 *   Queue functions (not thread-safe).
 *
 * PUBLIC FUNCTIONS:
 *   queue_create_empty
 *   queue_create_full
 *   queue_delete
 *
 *   queue_is_empty
 *   queue_is_full
 *
 *   queue_dequeue
 *   queue_enqueue
 *
 * AUTHOR: RVC       START DATE: 15/03/2023
 *
 * CHANGES:
 * 
 ******************************************************************************/

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdint.h>
#include <stdbool.h>

/******************************************************************************
 *                            STRUCTURE DEFINITION                            *
 ******************************************************************************/

/* Circular queue data structure.
 *
 * https://www.programiz.com/dsa/queue
 * https://www.programiz.com/dsa/circular-queue */
typedef struct
{
    /* An array which contains 'elm_cnt' elements of 'elm_size' bytes each */
    void * p_array;

    int elm_cnt;
    int elm_size;

    /* An index which tracks the first element in queue */
    int front_idx;

    /* An index which tracks the last element in queue */
    int rear_idx;

} queue_t;

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Return an empty queue whose size is ('elm_cnt' * 'elm_size') bytes.
 * Note: The queue must be deleted when no longer used */
queue_t queue_create_empty(uint32_t elm_cnt, uint32_t elm_size);

/* Return a full queue whose content is a shallow copy of 'p_array'.
 * Note: The queue must be deleted when no longer used */
queue_t queue_create_full(const void * p_array,
                          uint32_t elm_cnt, uint32_t elm_size);

/* Delete queue.
 * Note: This function will deallocate 'p_queue->p_array' */
void queue_delete(queue_t * p_queue);

/* Return true if queue is empty. Otherwise, return false */
bool queue_is_empty(const queue_t * p_queue);

/* Return true if queue is full. Otherwise, return false */
bool queue_is_full(const queue_t * p_queue);

/* Remove an element from the front of the queue. Then, return its address.
 * Note: Return non-NULL value if successful */
void * queue_dequeue(queue_t * p_queue);

/* Add a shallow copy of element (pointed by 'p_elm') to the end of the queue.
 * Return true if successful. Otherwise, return false.
 *
 * Note: Size of the element must be equal to 'p_queue->elm_size' */
bool queue_enqueue(queue_t * p_queue, const void * p_elm);

#endif /* _QUEUE_H_ */
