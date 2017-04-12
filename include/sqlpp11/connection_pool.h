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

#ifndef SQLPP_CONNECTION_POOL_H
#define SQLPP_CONNECTION_POOL_H

#include <mutex>
#include <stack>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <chrono>
#include <exception>
#include <future>
#include <type_traits>
#include <sqlpp11/exception.h>
#include <sqlpp11/pool_connection.h>
#include <sqlpp11/bind.h>
#include <sqlpp11/future.h>

namespace sqlpp
{
  namespace connection_validator
  {
    struct automatic
    {
      template<typename Connection>
      void validate(Connection* connection)
      {
        if (!connection->is_valid())
        {
          try
          {
            connection->reconnect();
          }
          catch (const sqlpp::exception&)
          {
            throw sqlpp::exception("Failed to reconnect to database.");
          }
        }
      }

      template<typename Connection>
      void deregister(Connection* connection) {}
    };

    using namespace std::chrono_literals;
    class periodic
    {
    private:
      std::chrono::seconds revalidate_interval;
      std::unordered_map<void*, std::chrono::time_point<std::chrono::system_clock>> last_checked;

    public:
      periodic(const std::chrono::seconds r = 28800s) //default wait_timeout in MySQL
        : revalidate_interval(r), last_checked() {}

      template<typename Connection>
      void validate(Connection* connection)
      {
        auto last = last_checked.find(connection);
        auto now = std::chrono::system_clock::now();
        if (last == last_checked.end())
        {
          last_checked.emplace_hint(last, connection, now);
        }

        if (now - last->second < revalidate_interval)
        {
          return;
        }

        if (!connection->is_valid())
        {
          try
          {
            connection->reconnect();
          }
          catch (const sqlpp::exception& e)
          {
            throw sqlpp::exception("Failed to reconnect to database.");
          }
        }

        last = now;
      }

      template<typename Connection>
      void deregister(Connection* con)
      {
        auto itr = last_checked.find(con);
        if (itr != last_checked.end())
        {
          last_checked.erase(itr);
        }
      }
    };

    struct none
    {
      template<typename Connection>
      void validate(Connection*) {}

      template<typename Connection>
      void deregister(Connection*) {}
    };
  }

  template <typename Connection_config,
    typename Connection_validator = connection_validator::automatic,
    typename Connection = typename std::enable_if<std::is_class<Connection_config::connection>::value, Connection_config::connection>::type>
  class connection_pool_t
  {
    friend pool_connection<Connection_config, Connection_validator, Connection>;

  private:
    std::mutex connection_pool_mutex;
    const std::shared_ptr<Connection_config> config;
    size_t maximum_pool_size = 0;
    std::stack<std::unique_ptr<Connection>> free_connections;
    Connection_validator connection_validator;

    void free_connection(std::unique_ptr<Connection>& connection)
    {
      std::lock_guard<std::mutex> lock(connection_pool_mutex);
      if (free_connections.size() >= maximum_pool_size)
      {
        // Exceeds default size, deregister left over info in the connection_validator and let connection self destroy.
        connection_validator.deregister(connection.get());
      }
      else
      {
        if (connection.get())
        {
          free_connections.push(std::move(connection));
        }
        else
        {
          throw sqlpp::exception("Trying to free an empty connection.");
        }
      }
    }

  public:
    connection_pool_t(const std::shared_ptr<Connection_config>& config, size_t pool_size)
      : config(config), maximum_pool_size(pool_size), connection_validator(Connection_validator()) {}
    ~connection_pool_t() = default;
    connection_pool_t(const connection_pool_t&) = delete;
    connection_pool_t(connection_pool_t&& other)
      : config(std::move(other.config)), maximum_pool_size(std::move(other.maximum_pool_size)),
      connection_validator(std::move(other.connection_validator)) {}
    connection_pool_t& operator=(const connection_pool_t&) = delete;
    connection_pool_t& operator=(connection_pool_t&&) = delete;

    typedef Connection_config connection_config_t;
    typedef Connection_validator connection_validator_t;
    typedef Connection connection_t;
    typedef pool_connection<Connection_config,Connection_validator,Connection> pool_connection_t;

    template<typename Query>
    using query_result = sqlpp::query_future_result_t<connection_pool_t,Query>;

    template<typename Query>
    using query_promise = sqlpp::query_promise<connection_pool_t,Query>;

    template<typename Query>
    using query_future = sqlpp::query_future<connection_pool_t,Query>;

    auto get_connection()
      -> pool_connection<Connection_config, Connection_validator, Connection> 
    {
      std::lock_guard<std::mutex> lock(connection_pool_mutex);
      while (true)
      {
        try
        {
          if (!free_connections.empty())
          {
            auto connection = std::move(free_connections.top());
            free_connections.pop();
            connection_validator.validate(connection.get());

            return pool_connection<Connection_config, Connection_validator, Connection>(std::move(connection), this);
          }
          else
          {
            break;
          }
        }
        catch (const sqlpp::exception&)
        {
          throw sqlpp::exception("Failed to retrieve a valid connection.");
        }
      }

      try
      {
        return pool_connection<Connection_config, Connection_validator, Connection>(std::move(std::make_unique<Connection>(config)), this);
      }
      catch (const sqlpp::exception&)
      {
        throw sqlpp::exception("Failed to spawn a new connection.");
      }
    }

    template<typename Query, typename Lambda>
    void operator()(Query query, Lambda callback)
    {
      sqlpp::bind(*this, query, callback)();
    }

    template<typename Query>
    void operator()(Query query)
    {
      operator()(query, []() {});
    }

    template<typename Query>
    static void execute_query_promise(query_promise<Query>& promise, connection_pool_t& pool, Query query)
    {
      try {
        auto connection = pool.get_connection();
        auto result = connection(query);
        promise.set_value(std::move(query_result<Query>(std::move(connection), std::move(result))));
      } catch(...) {
        promise.set_exception(std::current_exception());
      }
    }

    template<typename Query>
    query_promise<Query> make_query_promise() { return query_promise<Query>(); }
  };

  template<typename Connection_config,
    typename Connection_validator = connection_validator::automatic,
    typename Connection = typename std::enable_if<std::is_class<Connection_config::connection>::value, Connection_config::connection>::type>
  auto connection_pool(const std::shared_ptr<Connection_config>& config, size_t max_pool_size)
    -> connection_pool_t<Connection_config, Connection_validator, Connection>
  {
    return connection_pool_t<Connection_config, Connection_validator, Connection>(config, max_pool_size);
  }
}

#endif
