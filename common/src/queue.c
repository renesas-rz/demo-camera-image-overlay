/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: queue.c
 *
 * DESCRIPTION:
 *   Queue function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'queue.h'.
 * 
 * AUTHOR: RVC       START DATE: 15/03/2023
 *
 ******************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

queue_t queue_create_empty(uint32_t elm_cnt, uint32_t elm_size)
{
    queue_t queue;

    /* Front = -2
     * ↓
     * ┌┄┄┄┄┬┄┄┄┄┬────┬────┬────┬────┬────┬────┬────┬────┐ elm_cnt  = 4
     * ┊    ┊    │    │    │    │    │    │    │    │    │ elm_size = 2
     * └┄┄┄┄┴┄┄┄┄┴────┴────┴────┴────┴────┴────┴────┴────┘
     * ↑         ├─────────┼─────────┼─────────┼─────────┤
     * Rear = -2  Element 1 Element 2 Element 3 Element 4 */

    /* Check parameters */
    assert((elm_cnt > 0) && (elm_size > 0));

    queue.elm_cnt  = (int)elm_cnt;
    queue.elm_size = (int)elm_size;
    queue.p_array  = calloc(elm_cnt, elm_size);

    /* Mark the queue as empty */
    queue.front_idx = -1 * (int)elm_size;
    queue.rear_idx  = -1 * (int)elm_size;

    return queue;
}

queue_t queue_create_full(const void * p_array,
                          uint32_t elm_cnt,
                          uint32_t elm_size)
{
    queue_t queue;

    /* Front = 0                           Rear = 6
     * ↓                                   ↓
     * ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐ elm_cnt  = 4
     * │  A  │  B  │  C  │  D  │  E  │  F  │  G  │  H  │ elm_size = 2
     * └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
     * ├───────────┼───────────┼───────────┼───────────┤
     *   Element 1   Element 2   Element 3   Element 4 */

    /* Check parameters */
    assert((p_array != NULL) && (elm_cnt > 0) && (elm_size > 0));

    /* Create empty queue */
    queue = queue_create_empty(elm_cnt, elm_size);

    /* Copy data from 'p_array' to 'queue.p_array' */
    memcpy(queue.p_array, p_array, elm_cnt * elm_size);

    /* Mark the queue as full */
    queue.front_idx = 0;
    queue.rear_idx  = (queue.elm_cnt - 1) * queue.elm_size;

    return queue;
}

void queue_delete(queue_t * p_queue)
{
    /* Check parameter */
    assert(p_queue != NULL);

    /* Free entire array */
    free(p_queue->p_array);

    /* Deinitialize structure */
    p_queue->p_array  = NULL;
    p_queue->elm_cnt  = 0;
    p_queue->elm_size = 0;

    p_queue->front_idx = -1;
    p_queue->rear_idx  = -1;
}

bool queue_is_empty(const queue_t * p_queue)
{
    /* Check parameter */
    assert(p_queue != NULL);

    /* Case 1: After calling function 'queue_create_empty'.
     * Case 2: After dequeuing the last element (function 'queue_dequeue'):
     *
     *   Code snippets:
     *
     *     int16_t array[2] = { 1, 2 };
     *     int16_t value1 = 0;
     *     int16_t value2 = 0;
     *
     *     queue_t queue = queue_create_empty(3, sizeof(int16_t));
     *     queue_enqueue(&queue, &(array[0]));
     *     queue_enqueue(&queue, &(array[1]));
     *
     *     value1 = *((int16_t *)queue_dequeue(&queue));
     *     value2 = *((int16_t *)queue_dequeue(&queue));
     *
     *   Result:
     *
     *     Front = -2
     *     ↓
     *     ┌┄┄┄┄┬┄┄┄┄┬─────┬─────┬─────┬─────┬─────┬─────┐ elm_cnt  = 3
     *     │    ┊    │  A  │  B  │  C  │  D  │     │     │ elm_size = 2
     *     └┄┄┄┄┴┄┄┄┄┴─────┴─────┴─────┴─────┴─────┴─────┘
     *     ↑         ├───────────┼───────────┼───────────┤
     *     Rear = -2   Element 1   Element 2   Element 3 */

    return (p_queue->front_idx == (-1 * p_queue->elm_size));
}

bool queue_is_full(const queue_t * p_queue)
{
    /* Check parameter */
    assert(p_queue != NULL);

    /* Case 1: When latest element is at the end of queue and the app did not
     * dequeue any elements from it (see function 'queue_create_full').
     *
     * Case 2: When latest element is right after oldest element:
     *
     *   Code snippets:
     *
     *     int16_t array[4] = { 1, 2, 3, 4 };
     *
     *     queue_t queue = queue_create_full(array,
     *                                       sizeof(array) / sizeof(int16_t),
     *                                       sizeof(int16_t));
     *     int16_t value1 = *((int16_t *)queue_dequeue(&queue));
     *
     *     int16_t value2 = 5;
     *     queue_enqueue(&queue, &value2);
     *
     *   Result:
     *
     *     Rear = 0    Front = 2
     *     ↓           ↓
     *     ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐ elm_cnt  = 4
     *     │  I  │  K  │  C  │  D  │  E  │  F  │  G  │  H  │ elm_size = 2
     *     └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
     *     ├───────────┼───────────┼───────────┼───────────┤
     *       Element 1   Element 2   Element 3   Element 4 */

    const int last_idx = (p_queue->elm_cnt - 1) * p_queue->elm_size;

    return ((p_queue->front_idx == 0) && (p_queue->rear_idx == last_idx)) ||
           (p_queue->front_idx == (p_queue->rear_idx + p_queue->elm_size));
}

void * queue_dequeue(queue_t * p_queue)
{
    void * p_elm = NULL;

    if (queue_is_empty(p_queue))
    {
        printf("Error: Queue is empty\n");
    }
    else
    {
        p_elm = p_queue->p_array + p_queue->front_idx;

        if (p_queue->front_idx == p_queue->rear_idx)
        {
            /* Initialize the queue if there is only 1 element in it */
            p_queue->front_idx = -1 * p_queue->elm_size;
            p_queue->rear_idx  = -1 * p_queue->elm_size;
        }
        else
        {
            /* Adjust 'front_idx' for next function call 'queue_dequeue' */
            p_queue->front_idx = (p_queue->front_idx + p_queue->elm_size) %
                                 (p_queue->elm_cnt * p_queue->elm_size);
        }
    }

    return p_elm;
}

bool queue_enqueue(queue_t * p_queue, const void * p_elm)
{
    bool is_success = true;

    if (queue_is_full(p_queue))
    {
        printf("Error: Queue is full\n");
        is_success = false;
    }
    else
    {
        if (p_queue->front_idx == (-1 * p_queue->elm_size))
        {
            /* The first element is about to be added to the queue.
             * Let's adjust 'front_idx' for next function call 'queue_dequeue'
             */
            p_queue->front_idx = 0;
        }

        /* Add element to the end of the queue */
        p_queue->rear_idx = (p_queue->rear_idx + p_queue->elm_size) %
                            (p_queue->elm_cnt * p_queue->elm_size);

        memcpy(p_queue->p_array + p_queue->rear_idx, p_elm, p_queue->elm_size);
    }

    return is_success;
}
