/*
 * Copyright (c) 2013-2017, Roland Bock, Aaron Bishop
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include "Sample.h"
#include "MockDb.h"
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/connection_pool.h>
#define TEST_ASIO
#ifdef TEST_ASIO
#include <asio.hpp>
#include <sqlpp11/boost_async.h>
using asio::io_service;
#elif defined(TEST_BOOST_ASIO)
#include <boost/asio.hpp>
#include <sqlpp11/boost_async.h>
using boost::asio::io_service;
#endif
int Async(int, char *[])
{
  auto config = std::make_shared<MockDbConfig>();
  sqlpp::connection_pool_t<MockDbConfig,sqlpp::connection_validator::none> pool(config, 1);
  const auto t = test::TabBar{};
  try 
  {
    //Test pool
    {
      pool.get_connection()(insert_into(t).set(t.beta = "string", t.gamma = false));
    }
    auto query = select(all_of(t)).from(t).unconditionally();
//     {
//       std::mutex m;
//       std::unique_lock<std::mutex> lock(m);
//       auto unlock_callback = [&lock](){ lock.unlock(); };
//       pool(query, unlock_callback);
//       lock.lock();
//     }
    {
      auto promise = decltype(pool)::make_query_promise(query);
      auto future = promise.get_future();
      std::thread(decltype(pool)::execute_query_promise<decltype(query)>, std::move(promise), std::ref(pool), query).detach();
      auto result = future.get();
      for(const auto& row : result.result) {
        std::cout << row.beta << ' ' << row.gamma << '\n';
      }
    }
    {
      auto future = pool(query);
      auto result = future.get();
      for(const auto& row : result.result) {
        std::cout << row.beta << ' ' << row.gamma << '\n';
      }
    }
#if defined(TEST_ASIO) || defined(TEST_BOOST_ASIO)
    {
      io_service io;
      io_service::work work(io);
      std::thread t([&io](){io.run();});
      auto future = sqlpp::io_runner_t<io_service>::async(io, pool, query);
      auto result = future.get();
      for(const auto& row : result.result) {
        std::cout << row.beta << ' ' << row.gamma << '\n';
      }
      io.stop();
      t.join();
    }
#endif
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
