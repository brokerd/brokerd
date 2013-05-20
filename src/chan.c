// This file is part of the "fyrehose" project
//   (c) 2011-2013 Paul Asmuth <paul@paulasmuth.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <stdlib.h>
#include <pthread.h>

#include "chan.h"

chan_t* global_channel;

chan_t* chan_init(/*char* key, int key_len*/) {
  chan_t* self = malloc(sizeof(chan_t *));
  self->sublist = NULL;
  pthread_mutex_init(&self->lock, NULL);
  return self;
}

chan_t* chan_lookup(char* key, int key_len) {
  return global_channel;
}

void chan_subscribe(chan_t* self, conn_t* conn) {
  pthread_mutex_lock(&self->lock);

  conn->channel  = self;
  conn->next_sub = self->sublist;
  self->sublist  = conn;

  pthread_mutex_unlock(&self->lock);
}

void chan_unsubscribe(chan_t* self, conn_t* conn) {
  pthread_mutex_lock(&self->lock);
  printf("close!\n");

  conn_t** cur = &self->sublist;
  conn->channel = NULL;

  while(*cur != NULL && (*cur)->sock != conn->sock)
    cur = &((*cur)->next_sub);

  if (*cur)
    *cur = (*cur)->next_sub;

  pthread_mutex_unlock(&self->lock);
}
