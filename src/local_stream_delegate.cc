#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_STREAM_DELEGATE_IPP_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_STREAM_DELEGATE_IPP_20170307

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <functional>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include "local_stream_delegate.hpp"

boost::network::http::impl::local_stream_delegate::local_stream_delegate(
    boost::asio::io_service &service)
    : service_(service) {}

void boost::network::http::impl::local_stream_delegate::connect(
    boost::asio::local::stream_protocol::endpoint &endpoint,
    std::function<void(boost::system::error_code const &)> handler) {

  socket_.reset(new boost::asio::local::stream_protocol::socket(service_));
  socket_->async_connect(endpoint, handler);
}

void boost::network::http::impl::local_stream_delegate::write(
    boost::asio::streambuf &command_streambuf,
    std::function<void(boost::system::error_code const &, size_t)> handler) {
  boost::asio::async_write(*socket_, command_streambuf, handler);
}

void boost::network::http::impl::local_stream_delegate::read_some(
    boost::asio::mutable_buffers_1 const &read_buffer,
    std::function<void(boost::system::error_code const &, size_t)> handler) {
  socket_->async_read_some(read_buffer, handler);
}

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_STREAM_DELEGATE_IPP_20170307