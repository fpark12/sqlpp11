/*
* Copyright (c) 2013 - 2017, Roland Bock, Frank Park, Aaron Bishop
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
*   Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
*   Redistributions in binary form must reproduce the above copyright notice, this
*   list of conditions and the following disclaimer in the documentation and/or
*   other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#ifndef SQLPP_FUTURE_H
#define SQLPP_FUTURE_H

#include <exception>
#include <future>

namespace sqlpp {
  template<typename Connection, typename Query, typename Result =
#if __cplusplus > 201402L
    std::invoke_result<Connection(Query)>
#else
    std::result_of<Connection(Query)>
#endif
  >
  struct query_future_result_t {
    typedef Connection connection_t;
    typedef Query query_t;
    typedef Result result_t;

    connection_t connection;
    result_t result;

    query_future_result_t(connection_t&& con, result_t&& res)
      : connection(std::move(con)), result(std::move(res))
    {}
  };

  template<typename Connection, typename Query>
  using query_future = std::future<query_future_result_t<Connection,Query> >;

  template<typename Connection, typename Query>
  using query_promise = std::promise<query_future_result_t<Connection,Query> >;
}

#endif
