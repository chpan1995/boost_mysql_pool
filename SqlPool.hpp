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

#include <boost/describe/class.hpp>
#include <boost/describe.hpp>
// #include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
// #include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/tcp_ssl.hpp>


#include <boost/asio/io_context.hpp>
// #include <boost/asio/thread_pool.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/system/system_error.hpp>


#include <array>
#include <queue>
#include <type_traits>
#include <vector>


namespace desc = boost::describe;

template<class T, template<class...> class L, class... D>
auto struct_to_tuple_impl( T const& t, L<D...> )
{
    return std::make_tuple( t.*D::pointer... );
}

template<class T,
         class Dm = desc::describe_members<T,
                                           desc::mod_public | desc::mod_inherited>,
         class En = std::enable_if_t<!std::is_union<T>::value> >
auto struct_to_tuple( T const& t )
{
    return struct_to_tuple_impl( t, Dm() );
}


template <typename T> struct IsTuple {
    static constexpr bool value = false;
};

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
    } else if constexpr(is_std_Tuple_v<T>) {
        return std::tuple_cat(value);
    } else {
        return std::make_tuple(value);
    }
}

// 递归展开函数，将参数解包到 tuple
template <typename T, typename... Args>
auto packToTuple(T first, Args... args) {
    auto tail = packToTuple(args...); // 包不止一个参数递归调用 最后一个包进入packToTuple(T value)
    return std::tuple_cat([&]{
        if constexpr (is_std_array_v<T>) {
            return arrayToTuple(first);
        } else if constexpr(is_std_Tuple_v<T>) {
            return std::tuple_cat(first);
        } else {
            return std::make_tuple(first);
        }
    }(), tail);
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

struct connode {
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tls_client};
    boost::asio::ip::tcp::resolver resolver{ctx.get_executor()};
    std::shared_ptr<boost::mysql::tcp_ssl_connection> con;
    connode(std::string ip, std::uint32_t port, std::string username,
            std::string password, std::string database) {
        auto endpoints = resolver.resolve(ip, std::to_string(port));
        boost::mysql::handshake_params params(username, // username
                                              password, // password
                                              database);
        con = std::make_shared<boost::mysql::tcp_ssl_connection>(ctx.get_executor(),
                                                                 ssl_ctx);
        con->connect(*endpoints.begin(), params);
    }
    ~connode() {
        con->close();
    }
};

class SqlPool {
public:
    void connect(std::string ip, std::uint32_t port, std::string username,
                 std::string password, std::string database);

    template <typename T, typename... Args>
    void query(std::string sql, T &res, Args... args);

    template <typename T, typename... Args>
    void query(std::shared_ptr<connode>,std::string sql, T &res, Args... args);

    template <typename... Args>
    void executor(std::string sql, boost::mysql::results &res, Args... args);

    std::shared_ptr<connode> startTransaction();
    void commit(std::shared_ptr<connode> conn);
public:
    static SqlPool* instance();
    ~SqlPool();

private:
    std::shared_ptr<connode> getCon();
    void releaseCon(std::shared_ptr<connode> con);
    SqlPool();

private:
    std::queue<std::shared_ptr<connode>> m_cons;
    std::mutex m_mtx;
    boost::asio::io_context m_ctx;

    // TO DO
    // boost::shared_ptr<boost::mysql::connection_pool> m_pool;
    // boost::asio::thread_pool m_th_pool{4};
};
template <typename T, typename... Args>
void SqlPool::query(std::string sql, T &res, Args... args) {
    auto conn = getCon();
    try {
        if (conn) {
            boost::mysql::statement stmt = conn->con->prepare_statement(sql);
            if constexpr (sizeof...(args) == 0) {
                conn->con->execute(stmt.bind(), res);
            } else {
                conn->con->execute(stmt.bind(packToTuple(args...)), res);
            }
            releaseCon(conn);
        }
    } catch (const boost::mysql::error_with_diagnostics &err) {
        std::cout << "Operation failed with error code: " << err.code().value() << "----->" << err.what() << '\n'
                << "Server diagnostics: "
                << err.get_diagnostics().server_message().data();
        if (conn)
            releaseCon(conn);
    } catch (const std::exception &err) {
        std::cerr << "Error: " << err.what();
        if (conn)
            releaseCon(conn);
    }
}

template <typename T, typename... Args>
void SqlPool::query(std::shared_ptr<connode> conn,std::string sql, T &res, Args... args)
{
    if (conn) {
        boost::mysql::statement stmt = conn->con->prepare_statement(sql);
        if constexpr (sizeof...(args) == 0) {
            conn->con->execute(stmt.bind(), res);
        } else {
            conn->con->execute(stmt.bind(packToTuple(args...)), res);
        }
    }
}

template <typename... Args>
void SqlPool::executor(std::string sql, boost::mysql::results &res,
                       Args... args) {
    try {
        // boost::mysql::diagnostics diag;
        // boost::mysql::error_code ec;
        // boost::asio::yield_context yield;
        // m_pool->async_get_connection_impl();
        // auto con = m_pool->async_get_connection(diag,yield[ec]);
        // mysql::throw_on_error(ec, diag);
    } catch (const std::exception &e) {
        std::cerr << e.what();
    }
}
}
#endif // SQLPOOL_H
