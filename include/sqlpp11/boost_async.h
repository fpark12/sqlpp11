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

#ifndef SQLPP_BOOST_ASYNC_H
#define SQLPP_BOOST_ASYNC_H

#include <sqlpp11/connection_pool.h>
#include <memory>

namespace boost { namespace asio { class io_service; } }

namespace asio { class io_service; }

namespace sqlpp
{
  template<typename IO_Service>
  class io_runner_t {
  public:
    template<
      typename Connection_pool,
      typename Query
    >
    static auto async(IO_Service& service, Connection_pool& pool, Query query)
      -> query_future<typename Connection_pool::pool_connection_t,Query>
    {  
      typedef query_result<typename Connection_pool::pool_connection_t,Query> result_t;
      auto promise = std::make_shared<std::promise<result_t> >();
      service.post([promise,&pool,query](){
        try{
          auto connection = pool.get_connection();
          auto result = connection(query);
          promise->set_value(std::move(result_t(std::move(connection), std::move(result))));
        }catch(...){
          promise->set_exception(std::current_exception());
        }
      });
      return promise->get_future();
    }
  };
  
  typedef io_runner_t<boost::asio::io_service> io_runner;
  typedef io_runner_t<asio::io_service> asio_runner;
}

#endif
