/**
 * This file is part of the "libfnord" project
 *   Copyright (c) 2015 Paul Asmuth
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <libtransport/json/json.h>
#include <brokerd/util/time.h>
#include <brokerd/util/logging.h>
#include <brokerd/util/buffer.h>
#include <brokerd/http_server.h>
#include <brokerd/channel_map.h>

namespace brokerd {

HTTPServer::HTTPServer(
    ChannelMap* channel_map) :
    channel_map_(channel_map) {
  http_server_.setRequestHandler(
      std::bind(
          &HTTPServer::handleRequest,
          this,
          std::placeholders::_1,
          std::placeholders::_2));
}

ReturnCode HTTPServer::listenAndRun(const std::string& addr, int port) {
  logInfo("Starting HTTP server on $0:$1", addr, port);

  if (!http_server_.listen(addr, port)) {
    return ReturnCode::error("ERUNTIME", "listen() failed");
  }

  http_server_.run();
  return ReturnCode::success();
}

void HTTPServer::handleRequest(
    http::HTTPRequest* req,
    http::HTTPResponse* res) try {
  URI uri(req->uri());
  auto path = uri.path();
  auto path_parts = StringUtil::split(path.substr(1), "/");

  res->addHeader("Access-Control-Allow-Origin", "*");

  switch (req->method()) {

    case http::HTTPMessage::M_GET:
      if (path == "/ping") {
        handleRequest_PING(req, res);
        return;
      }

      if (path == "/serverid") {
        handleRequest_SERVERID(req, res);
        return;
      }

      if (path == "/stats") {
        handleRequest_STATS(req, res);
        return;
      }

      if (path_parts.size() == 2 &&
          path_parts[0] == "channel") {
        handleRequest_FETCH(req, res, path_parts[1]);
        return;
      }

      if (path_parts.size() == 3 &&
          path_parts[0] == "channel") {
        handleRequest_FETCH(req, res, path_parts[1], path_parts[2]);
        return;
      }

      if (path_parts.size() == 4 &&
          path_parts[0] == "channel" &&
          path_parts[3] == "next") {
        handleRequest_FETCH(req, res, path_parts[1], path_parts[2], true);
        return;
      }

      if (path_parts.size() == 5 &&
          path_parts[0] == "channel" &&
          path_parts[3] == "next") {
        handleRequest_FETCH(
            req,
            res,
            path_parts[1],
            path_parts[2],
            true,
            path_parts[4]);
        return;
      }

      break;

    case http::HTTPMessage::M_POST:
      if (path_parts.size() == 2 && path_parts[0] == "channel") {
        handleRequest_INSERT(req, res, path_parts[1]);
        return;
      }

      break;

  }

  res->setStatus(http::kStatusNotFound);
  res->addBody("not found");
} catch (const Exception& e) {
  res->setStatus(http::kStatusInternalServerError);
  res->addBody(StringUtil::format("error: $0: $1", e.getTypeName(), e.getMessage()));
}

void HTTPServer::handleRequest_INSERT(
    http::HTTPRequest* req,
    http::HTTPResponse* res,
    const std::string& channel_id_str) {
  if (req->body().empty()) {
    res->setStatus(http::kStatusBadRequest);
    res->addBody("error: empty message (body_size == 0)");
    return;
  }

  auto channel_id = ChannelID::fromString(channel_id_str);
  if (channel_id.isEmpty()) {
    res->setStatus(http::kStatusBadRequest);
    res->addBody("error: invalid channel id");
    return;
  }

  std::shared_ptr<Channel> channel;
  auto rc = channel_map_->findChannel(channel_id.get(), true, &channel);

  uint64_t offset = 0;
  if (rc.isSuccess()) {
    rc = channel->appendMessage(req->body(), &offset);
  }

  if (rc.isSuccess()) {
    res->addHeader("X-Broker-ServerID", channel_map_->getUID());
    res->addHeader("X-Broker-Created-Offset", StringUtil::toString(offset));
    res->setStatus(http::kStatusCreated);
    res->addBody("ok");
  } else {
    res->setStatus(http::kStatusInternalServerError);
    res->addBody(StringUtil::format("error: $0", rc.getMessage()));
  }
}

void HTTPServer::handleRequest_FETCH(
    http::HTTPRequest* req,
    http::HTTPResponse* res,
    const std::string& channel_id_str,
    const std::string& offset_str /* = "" */,
    bool next /* = false */,
    const std::string& batch_size_str /* = "" */) {
  auto channel_id = ChannelID::fromString(channel_id_str);
  if (channel_id.isEmpty()) {
    res->setStatus(http::kStatusBadRequest);
    res->addBody("error: invalid channel id");
    return;
  }

  std::shared_ptr<Channel> channel;
  {
    auto rc = channel_map_->findChannel(channel_id.get(), true, &channel);
    if (!rc.isSuccess()) {
      res->setStatus(http::kStatusBadRequest);
      res->addBody(StringUtil::format("error: $0", rc.getMessage()));
      return;
    }
  }

  uint64_t offset = 0;
  if (!offset_str.empty()) {
    try {
      offset = std::stoull(offset_str);
    } catch (...) {
      res->setStatus(http::kStatusBadRequest);
      res->addBody("error: invalid offset");
      return;
    }
  }

  uint64_t batch_size = 1;
  if (!batch_size_str.empty()) {
    try {
      batch_size = std::stoull(batch_size_str);
    } catch (...) {
      res->setStatus(http::kStatusBadRequest);
      res->addBody("error: invalid batch_size");
      return;
    }
  }

  if (next) {
    ++batch_size;
  }

  std::list<Message> messages;
  auto rc = channel->fetchMessages(offset, batch_size, &messages);
  if (!rc.isSuccess()) {
    res->setStatus(http::kStatusInternalServerError);
    res->addBody(StringUtil::format("error: $0", rc.getMessage()));
    return;
  }

  if (next && !messages.empty() && messages.front().offset == offset) {
    messages.pop_front();
  }

  res->setStatus(http::kStatusOK);
  res->addHeader("Content-Type", "application/json; charset=utf-8");
  res->addBody(toJSON(messages));
}

void HTTPServer::handleRequest_SERVERID(
    http::HTTPRequest* req,
    http::HTTPResponse* res) {
  auto serverid = channel_map_->getUID();
  res->addHeader("X-Broker-ServerID", serverid);
  res->addHeader("Content-Type", "text/plain");
  res->setStatus(http::kStatusOK);
  res->addBody(serverid);
}

void HTTPServer::handleRequest_STATS(
    http::HTTPRequest* req,
    http::HTTPResponse* res) {
  res->setStatus(http::kStatusOK);
  res->addHeader("Content-Type", "text/plain; charset=utf-8");
  res->addBody("");
}

void HTTPServer::handleRequest_PING(
    http::HTTPRequest* req,
    http::HTTPResponse* res) {
  res->setStatus(http::kStatusOK);
  res->addHeader("Content-Type", "text/plain; charset=utf-8");
  res->addBody("pong");
}

} // namespace brokerd

