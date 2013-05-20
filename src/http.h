// This file is part of the "fyrehose" project
//   (c) 2011-2013 Paul Asmuth <paul@paulasmuth.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef HTTP_H
#define HTTP_H

#define HTTP_STATE_METHOD 0
#define HTTP_STATE_URI 1
#define HTTP_STATE_VERSION 2
#define HTTP_STATE_HKEY 3
#define HTTP_STATE_HVAL 4

#define HTTP_METHOD_HEAD 0
#define HTTP_METHOD_GET 1
#define HTTP_METHOD_POST 2

#define HTTP_URI_LEN 1024
#define HTTP_URI_DEPTH 10

typedef struct {
  int   method;
  int   keepalive;
  char  uri[HTTP_URI_LEN];
  int   uri_len;
  char* uri_argv[HTTP_URI_DEPTH + 1];
  int   uri_argc;
  int   state;
  int   last_pos;
  char* cur_token;
  char* cur_hkey;
  int   cur_hkey_len;
} http_req_t;

http_req_t* http_req_init();
int http_read(http_req_t* req, char* buf, size_t len);
int http_read_method(http_req_t* req, char* method, int len);
int http_read_uri(http_req_t* req, char* uri, int len);
int http_read_version(http_req_t* req, char* version, int len);
void http_read_header(http_req_t* req, char* hkey, int hkey_len, char* hval, int hval_len);
void http_req_free(http_req_t* self);
void http_req_reset(http_req_t* self);

#endif
