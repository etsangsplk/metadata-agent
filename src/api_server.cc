/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include "api_server.h"

#include <boost/range/irange.hpp>

#include "configuration.h"
#include "http_common.h"
#include "logging.h"
#include "store.h"

namespace google {

MetadataApiServer::Dispatcher::Dispatcher(
    const HandlerMap& handlers, bool verbose)
    : handlers_(handlers), verbose_(verbose) {}

void MetadataApiServer::Dispatcher::operator()(
    const HttpServer::request& request,
    std::shared_ptr<HttpServer::connection> conn) {
  if (verbose_) {
    LOG(INFO) << "Dispatcher called: " << request.method
              << " " << request.destination
              << " headers: " << request.headers
              << " body: " << request.body;
  }
  // Look for the longest match first. This means going backwards through
  // the map, since strings are sorted in lexicographical order.
  for (auto it = handlers_.crbegin(); it != handlers_.crend(); --it) {
    const std::string& method = it->first.first;
    const std::string& prefix = it->first.second;
#ifdef VERBOSE
    LOG(DEBUG) << "Checking " << method << " " << prefix;
#endif
    if (request.method != method || request.destination.find(prefix) != 0) {
#ifdef VERBOSE
      LOG(DEBUG) << "No match; skipping " << method << " " << prefix;
#endif
      continue;
    }
#ifdef VERBOSE
    LOG(DEBUG) << "Handler found for " << request.method
               << " " << request.destination;
#endif
    const Handler& handler = it->second;
    handler(request, conn);
  }
}

void MetadataApiServer::Dispatcher::log(const HttpServer::string_type& info) {
  LOG(ERROR) << info;
}


MetadataApiServer::MetadataApiServer(const Configuration& config,
                                     const MetadataStore& store,
                                     int server_threads,
                                     const std::string& host, int port)
    : config_(config), store_(store), dispatcher_({
        {{"GET", "/monitoredResource/"},
         [=](const HttpServer::request& request,
             std::shared_ptr<HttpServer::connection> conn) {
             HandleMonitoredResource(request, conn);
         }},
      }, config_.VerboseLogging()),
      server_(
          HttpServer::options(dispatcher_)
              .address(host)
              .port(std::to_string(port))),
      server_pool_()
{
  for (int i : boost::irange(0, server_threads)) {
    server_pool_.emplace_back(&HttpServer::run, &server_);
  }
}

MetadataApiServer::~MetadataApiServer() {
  for (auto& thread : server_pool_) {
    thread.join();
  }
}

void MetadataApiServer::HandleMonitoredResource(
    const HttpServer::request& request,
    std::shared_ptr<HttpServer::connection> conn) {
  // The format for the local metadata API request is:
  //   {host}:{port}/monitoredResource/{id}
  static const std::string kPrefix = "/monitoredResource/";;
  const std::string id = request.destination.substr(kPrefix.size());
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Handler called for " << id;
  }
  try {
    const MonitoredResource& resource = store_.LookupResource(id);
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Found resource for " << id << ": " << resource;
    }
    conn->set_status(HttpServer::connection::ok);
    conn->set_headers(std::map<std::string, std::string>({
      {"Content-Type", "application/json"},
    }));
    conn->write(resource.ToJSON()->ToString());
  } catch (const std::out_of_range& e) {
    // TODO: This could be considered log spam.
    // As we add more resource mappings, these will become less and less
    // frequent, and could be promoted to ERROR.
    if (config_.VerboseLogging()) {
      LOG(WARNING) << "No matching resource for " << id;
    }
    conn->set_status(HttpServer::connection::not_found);
    conn->set_headers(std::map<std::string, std::string>({
      {"Content-Type", "application/json"},
    }));
    json::value json_response = json::object({
      {"status_code", json::number(404)},
      {"error", json::string("Not found")},
    });
    conn->write(json_response->ToString());
  }
}

}
