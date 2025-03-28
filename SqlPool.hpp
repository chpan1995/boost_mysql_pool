/**
 * @file SqlPool.h
 * @author chenghao pan (panchenghao@gaozhe.com.cn)
 * @brief
 * @version 0.1
 * @date 2024-04-22
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SQLPOOL_H
#define SQLPOOL_H

#include <boost/describe.hpp>
#include <boost/describe/class.hpp>
#include <iostream>

#undef emit // 禁用 emit 宏
#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>
#define emit // 重新启用 Qt 的 emit 宏

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <queue>
#include <type_traits>
#include <vector>

namespace desc = boost::describe;

template <class T, template <class...> class L, class... D>
auto struct_to_tuple_impl(T const &t, L<D...>) {
  return std::make_tuple(t.*D::pointer...);
}

template <class T,
          class Dm =
              desc::describe_members<T, desc::mod_public | desc::mod_inherited>,
          class En = std::enable_if_t<!std::is_union<T>::value>>
auto struct_to_tuple(T const &t) {
  return struct_to_tuple_impl(t, Dm());
}

template <typename T> struct IsTuple { static constexpr bool value = false; };

template <typename... Ts> struct IsTuple<std::tuple<Ts...>> {
  static constexpr bool value = true;
};

// 辅助函数，用于简化使用
template <typename T> constexpr bool is_std_Tuple_v = IsTuple<T>::value;

// 针对 std::vector 特化模板类
template <typename T> struct is_vector : std::false_type {};

// 针对 std::vector 特化模板类
template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};

// 辅助函数，用于简化使用
template <typename T> constexpr bool is_vector_v = is_vector<T>::value;

// 自定义的类型特征模板，用于检查是否是 std::array
template <typename T> struct is_std_array : std::false_type {};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

// 辅助函数，用于简化使用
template <typename T> constexpr bool is_std_array_v = is_std_array<T>::value;

template <typename T> void sqlbind(boost::mysql::statement &stmt, T arg) {
  if constexpr (is_vector_v<T>) {
    for (auto &it : arg) {
      stmt.bind(it);
    }
  } else {
    stmt.bind(arg);
  }
}

template <typename T, typename... Types>
void sqlbind(boost::mysql::statement &stmt, T first, Types &&...args) {
  sqlbind(stmt, first);
  sqlbind(stmt, args...);
}

// 递归展开函数，将 std::array 中的元素递归地放入 tuple 中
template <typename T, std::size_t N, std::size_t... Is>
auto arrayToTupleHelper(const std::array<T, N> &arr,
                        std::index_sequence<Is...>) {
  return std::make_tuple(arr[Is]...);
}

// 将 std::array 中的元素放入 tuple 中
template <typename T, std::size_t N>
auto arrayToTuple(const std::array<T, N> &arr) {
  return arrayToTupleHelper(arr, std::make_index_sequence<N>{});
}

// 递归终止函数，将最后一个参数存入 tuple
template <typename T> auto packToTuple(T value) {
  if constexpr (is_std_array_v<T>) {
    return arrayToTuple(value);
  } else if constexpr (is_std_Tuple_v<T>) {
    return std::tuple_cat(value);
  } else {
    return std::make_tuple(value);
  }
}

// 递归展开函数，将参数解包到 tuple
template <typename T, typename... Args>
auto packToTuple(T first, Args... args) {
  auto tail = packToTuple(
      args...); // 包不止一个参数递归调用 最后一个包进入packToTuple(T value)
  return std::tuple_cat(
      [&] {
        if constexpr (is_std_array_v<T>) {
          return arrayToTuple(first);
        } else if constexpr (is_std_Tuple_v<T>) {
          return std::tuple_cat(first);
        } else {
          return std::make_tuple(first);
        }
      }(),
      tail);
}

static std::string datateTostring(boost::mysql::datetime &datetime) {
  std::time_t time =
      std::chrono::system_clock::to_time_t(datetime.get_time_point());
  // 转换为tm结构用于输出
  std::tm tm = *std::localtime(&time);
  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}
namespace data_center {
class SqlPool {
public:
  template <typename T, typename... Args>
  void query(std::string sql, T &res, Args... args);

  template <typename T, typename... Args>
  void query(boost::mysql::pooled_connection &, std::string sql, T &res,
             Args... args);

  boost::mysql::pooled_connection startTransaction();
  void commit(boost::mysql::pooled_connection &conn);

public:
  static SqlPool *instance();
  ~SqlPool();

private:
  boost::mysql::pooled_connection getConn();
  SqlPool();

private:
  std::shared_ptr<boost::mysql::connection_pool> m_pool;
  boost::asio::thread_pool m_th_pool{8};
};
template <typename T, typename... Args>
void SqlPool::query(std::string sql, T &res, Args... args) {
  auto conn = getConn();
  if (conn.valid()) {
    try {
      boost::mysql::statement stmt = conn->prepare_statement(sql);
      if constexpr (sizeof...(args) == 0) {
        conn->execute(stmt.bind(), res);
      } else if constexpr (sizeof...(args) == 1) {
        if constexpr (is_vector_v<Args...>)
          conn->execute(stmt.bind((args.begin())..., (args.end())...), res);
        else
          conn->execute(stmt.bind(packToTuple(args...)), res);
      } else {
        conn->execute(stmt.bind(packToTuple(args...)), res);
      }
    } catch (const boost::mysql::error_with_diagnostics &err) {
      std::cout << "Operation failed with error code: " << err.code().value()
                << "----->" << err.what() << '\n'
                << "Server diagnostics: "
                << err.get_diagnostics().server_message().data();
      boost::system::error_code error;
      boost::mysql::diagnostics err_dia;
    } catch (const std::exception &err) {
      std::cerr << "Error: " << err.what();
      boost::system::error_code error;
      boost::mysql::diagnostics err_dia;
    }
    
  }
}

template <typename T, typename... Args>
void SqlPool::query(boost::mysql::pooled_connection &conn, std::string sql,
                    T &res, Args... args) {
  if (conn.valid()) {
    try {
      boost::mysql::statement stmt = conn->prepare_statement(sql);
      if constexpr (sizeof...(args) == 0) {
        conn->execute(stmt.bind(), res);
      } else if constexpr (sizeof...(args) == 1) {
        if constexpr (is_vector_v<Args...>)
          conn->execute(stmt.bind((args.begin())..., (args.end())...), res);
        else
          conn->execute(stmt.bind(packToTuple(args...)), res);
      } else {
        conn->execute(stmt.bind(packToTuple(args...)), res);
      }
    } catch (const boost::mysql::error_with_diagnostics &err) {
      std::cout << "Operation failed with error code: " << err.code().value()
                << "----->" << err.what() << '\n'
                << "Server diagnostics: "
                << err.get_diagnostics().server_message().data();
      boost::mysql::results result;
      conn->execute("ROLLBACK", result);
      boost::throw_exception(err);
    } catch (const std::exception &err) {
      std::cerr << "Error: " << err.what();
      boost::mysql::results result;
      conn->execute("ROLLBACK", result);
      throw std::exception(err);
    }
  }
}

} // namespace data_center
#endif // SQLPOOL_H
