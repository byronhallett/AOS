/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <stdlib.h>
#include <stdint.h>
#include <clock/clock.h>

/* The functions in src/device.h should help you interact with the timer
 * to set registers and configure timeouts. */
#include "device.h"


struct task
{
  uint32_t id;
  uint64_t trigger_time;
  struct task *next;
  timer_callback_t callback;
  void *data;
};

static struct
{
  volatile meson_timer_reg_t *regs;
  /* Add fields as you see necessary */
  int next_id;
  // struct timer *timers;
  struct task *task_head;
} clock;

int start_timer(unsigned char *timer_vaddr)
{
  int err = stop_timer();
  if (err != 0)
  {
    return err;
  }

  // Configure clock E
  clock.regs = (meson_timer_reg_t *)(timer_vaddr + TIMER_REG_START);
  configure_timestamp(clock.regs, TIMESTAMP_TIMEBASE_1_US);


  return CLOCK_R_OK;
}

timestamp_t get_time(void)
{
  return read_timestamp(clock.regs);
};

timestamp_t next_delay(void)
{
  if (clock.task_head == NULL)
    return -1;
  return clock.task_head->trigger_time - get_time();
}

/* place a node at the start of the list and reconfigure timeout */
void insert_first(struct task *t)
{
  // If this is inserted at the start of the list, re-configure the timeout
  clock.task_head = t;
  configure_timeout(clock.regs, MESON_TIMER_A, true, false, TIMEOUT_TIMEBASE_1_US, next_delay());
}


uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data)
{
  // compute trigger time before memory allocation for accuracy
  uint64_t trigger_time = get_time() + delay;
  int new_id = clock.next_id++;
  if (new_id == 0)
    new_id = clock.next_id++;

  // Create a new task node to insert
  struct task *new_task = (struct task *)malloc(sizeof(struct task));
  new_task->id = new_id;
  new_task->callback = callback;
  new_task->data = data;
  new_task->trigger_time = trigger_time;
  new_task->next = NULL;

  // now search the existing tasks and insert according to trigger time
  struct task *previous = NULL;
  struct task *current = clock.task_head;
  // base case, no nodes
  if (current == NULL) {
    // no searching occured, insert at front
    insert_first(new_task);
    return new_task->id;
  }
  // insert before first later trigger time
  while (current != NULL)
  {
    if (new_task->trigger_time < current->trigger_time)
    {
      new_task->next = current;
      if (previous != NULL)
      {
        previous->next = new_task;
      }
      else
      {
        insert_first(new_task);
      }
      // have inserted, can return early
      return new_task->id;
    }
    previous = current;
    current = current->next;
  }
  // was not inserted mid list, insert at end
  previous->next = new_task;
  return new_task->id;
}



int remove_timer(uint32_t id)
{
  // REMOVE THE TIMER WITH THE GIVEN ID
  struct task *previous = NULL;
  struct task *current = clock.task_head;
  while (current != NULL)
  {
    if (current->id == id)
    {
      // adjust pointers
      if (previous != NULL)
      {
        previous->next = current->next;
      }
      else
      {
        clock.task_head = current->next;
      }
      // free mem
      free(current);
      return CLOCK_R_OK;
    }
    // not right id, search list
    previous = current;
    current = current->next;
  }
  // id not found
  return CLOCK_R_OK;
}

int remove_all(void)
{
  // free all unconditionally
  while (clock.task_head != NULL)
  {
    struct task *old_task = clock.task_head;
    clock.task_head = clock.task_head->next;
    free(old_task);
  }
}

int stop_timer(void)
{
  /* Stop the timer from producing further interrupts and remove all
     * existing timeouts */

  // no tasks to start with
  remove_all();
  clock.next_id = 1;
  clock.task_head = NULL;


  // TODO: IS THIS THE RIGHT WAY TO STOP THE TIMEOUT?
  // configure_timeout(clock.regs, MESON_TIMER_A, false, false, TIMEOUT_TIMEBASE_1_US, 0);

  return CLOCK_R_OK;
}

int trigger_callback(struct task *t)
{
  if (t == NULL)
  {
    return CLOCK_R_CNCL;
  }
  timer_callback_t callback = t->callback;
  (*callback)(t->id, t->data);
  return CLOCK_R_OK;
}

int check_next_task()
{
  if (clock.task_head == NULL)
  {
    return CLOCK_R_CNCL;
  }
  int32_t time_diff = next_delay();

  // Trigger or re-schedule
  // If resched, just create a new timeout with time_diff
  if (time_diff > 0)
  {
    configure_timeout(clock.regs, MESON_TIMER_A, true, false, TIMEOUT_TIMEBASE_1_US, time_diff);
  }
  else
  {
    // if triggering, first write timeout with the next node, then use
    // move head to next before triggering
    struct task *current = clock.task_head;
    clock.task_head = current->next;
    if (clock.task_head != NULL)
    {
      configure_timeout(clock.regs, MESON_TIMER_A, true, false, TIMEOUT_TIMEBASE_1_US, next_delay());
    }
    // finally, fire the callback
    int status = trigger_callback(current);
    if (status != CLOCK_R_OK)
      return status;
  }

  return CLOCK_R_OK;
}

int timer_irq(
    void *data,
    seL4_Word irq, //which clock is this?
    seL4_IRQHandler irq_handler)
{
  /* Handle the IRQ */
  int status = check_next_task();

  /* Acknowledge that the IRQ has been handled */
  seL4_IRQHandler_Ack(irq_handler);
  return status;
}
