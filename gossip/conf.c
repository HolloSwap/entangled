/*
 * Copyright (c) 2018 IOTA Stiftung
 * https://github.com/iotaledger/entangled
 *
 * Refer to the LICENSE file for licensing information
 */

#include "gossip/conf.h"

retcode_t iota_gossip_conf_init(iota_gossip_conf_t* const conf) {
  if (conf == NULL) {
    return RC_NULL_PARAM;
  }

  memset(conf, 0, sizeof(iota_gossip_conf_t));

  conf->udp_receiver_port = DEFAULT_UDP_RECEIVER_PORT;
  conf->tcp_receiver_port = DEFAULT_TCP_RECEIVER_PORT;
  conf->mwm = DEFAULT_MWN;
  conf->neighbors = DEFAULT_NEIGHBORS;
  conf->p_select_milestone = DEFAULT_PROBABILITY_SELECT_MILESTONE;
  conf->p_remove_request = DEFAULT_PROBABILITY_REMOVE_REQUEST;

  return RC_OK;
}
