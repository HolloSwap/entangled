/*
 * Copyright (c) 2018 IOTA Stiftung
 * https://github.com/iotaledger/entangled
 *
 * Refer to the LICENSE file for licensing information
 */

#include <string.h>

#include "gossip/components/broadcaster.h"
#include "gossip/node.h"
#include "utils/containers/lists/concurrent_list_neighbor.h"
#include "utils/logger_helper.h"

#define BROADCASTER_LOGGER_ID "broadcaster"
#define BROADCASTER_TIMEOUT_SEC 5

/*
 * Private functions
 */

static void *broadcaster_routine(broadcaster_t *const broadcaster) {
  concurrent_list_node_neighbor_t *iter = NULL;
  flex_trit_t *transaction_flex_trits = NULL;

  if (broadcaster == NULL) {
    return NULL;
  }

  while (broadcaster->running) {
    lock_handle_lock(&broadcaster->lock);
    if (hash8019_queue_empty(broadcaster->queue)) {
      cond_handle_timedwait(&broadcaster->cond, &broadcaster->lock,
                            BROADCASTER_TIMEOUT_SEC);
    }
    transaction_flex_trits = hash8019_queue_peek(broadcaster->queue);
    if (transaction_flex_trits == NULL) {
      lock_handle_unlock(&broadcaster->lock);
      continue;
    }
    log_debug(BROADCASTER_LOGGER_ID, "Broadcasting transaction\n");
    if (broadcaster->node->neighbors) {
      iter = broadcaster->node->neighbors->front;
      while (iter) {
        if (neighbor_send(broadcaster->node, &iter->data,
                          transaction_flex_trits)) {
          log_warning(BROADCASTER_LOGGER_ID,
                      "Broadcasting transaction failed\n");
        }
        iter = iter->next;
      }
    }
    hash8019_queue_pop(&broadcaster->queue);
    lock_handle_unlock(&broadcaster->lock);
  }
  return NULL;
}

/*
 * Public functions
 */

retcode_t broadcaster_init(broadcaster_t *const broadcaster,
                           node_t *const node) {
  if (broadcaster == NULL || node == NULL) {
    return RC_NULL_PARAM;
  }

  logger_helper_init(BROADCASTER_LOGGER_ID, LOGGER_DEBUG, true);
  memset(broadcaster, 0, sizeof(broadcaster_t));
  broadcaster->running = false;
  broadcaster->node = node;
  broadcaster->queue = NULL;
  lock_handle_init(&broadcaster->lock);
  cond_handle_init(&broadcaster->cond);

  return RC_OK;
}

retcode_t broadcaster_destroy(broadcaster_t *const broadcaster) {
  if (broadcaster == NULL) {
    return RC_NULL_PARAM;
  } else if (broadcaster->running) {
    return RC_BROADCASTER_STILL_RUNNING;
  }

  broadcaster->node = NULL;
  hash8019_queue_free(&broadcaster->queue);
  lock_handle_destroy(&broadcaster->lock);
  cond_handle_destroy(&broadcaster->cond);
  logger_helper_destroy(BROADCASTER_LOGGER_ID);

  return RC_OK;
}

retcode_t broadcaster_start(broadcaster_t *const broadcaster) {
  if (broadcaster == NULL) {
    return RC_NULL_PARAM;
  }

  log_info(BROADCASTER_LOGGER_ID, "Spawning broadcaster thread\n");
  broadcaster->running = true;
  if (thread_handle_create(&broadcaster->thread,
                           (thread_routine_t)broadcaster_routine,
                           broadcaster) != 0) {
    log_critical(BROADCASTER_LOGGER_ID, "Spawning broadcaster thread failed\n");
    return RC_FAILED_THREAD_SPAWN;
  }

  return RC_OK;
}

retcode_t broadcaster_on_next(broadcaster_t *const broadcaster,
                              flex_trit_t const *const transaction_flex_trits) {
  retcode_t ret = RC_OK;

  if (broadcaster == NULL || transaction_flex_trits == NULL) {
    return RC_NULL_PARAM;
  }

  lock_handle_lock(&broadcaster->lock);
  ret = hash8019_queue_push(&broadcaster->queue, transaction_flex_trits);
  lock_handle_unlock(&broadcaster->lock);

  if (ret != RC_OK) {
    log_warning(BROADCASTER_LOGGER_ID,
                "Pushing transaction flex trits to broadcaster queue failed\n");
    return RC_BROADCASTER_FAILED_PUSH_QUEUE;
  } else {
    cond_handle_signal(&broadcaster->cond);
  }

  return RC_OK;
}

size_t broadcaster_size(broadcaster_t *const broadcaster) {
  size_t size = 0;

  if (broadcaster == NULL) {
    return 0;
  }

  lock_handle_lock(&broadcaster->lock);
  size = hash8019_queue_count(&broadcaster->queue);
  lock_handle_unlock(&broadcaster->lock);

  return size;
}

retcode_t broadcaster_stop(broadcaster_t *const broadcaster) {
  if (broadcaster == NULL) {
    return RC_NULL_PARAM;
  } else if (broadcaster->running == false) {
    return RC_OK;
  }

  log_info(BROADCASTER_LOGGER_ID, "Shutting down broadcaster thread\n");
  broadcaster->running = false;
  if (thread_handle_join(broadcaster->thread, NULL) != 0) {
    log_error(BROADCASTER_LOGGER_ID,
              "Shutting down broadcaster thread failed\n");
    return RC_FAILED_THREAD_JOIN;
  }

  return RC_OK;
}
