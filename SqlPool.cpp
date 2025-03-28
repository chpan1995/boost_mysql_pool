#include "SqlPool.hpp"
#include <boost/asio.hpp>

namespace data_center {

constexpr std::uint8_t POOL_SIZE = 10;

SqlPool::SqlPool(/* args */) {

  // connect(pconf->host(),pconf->port(),pconf->username(),"Gz-123456","ricesystem");
  boost::mysql::pool_params pool_prms{
      boost::mysql::host_and_port{"127.0.0.1", 3306},
      "gauture",
      "Gz-123456",
      "ricesystem",
  };
  pool_prms.initial_size = 2;
  pool_prms.max_size = 2;
  m_pool = std::make_shared<boost::mysql::connection_pool>(
      boost::mysql::pool_executor_params::thread_safe(m_th_pool.get_executor()),
      std::move(pool_prms));
  m_pool->async_run([this](boost::mysql::error_code ec) {
    if (ec) {
      std::cout << ec.message().c_str();
    } else {
      std::cout << "创建pool success";
    }
  });
}

SqlPool::~SqlPool() { m_pool->cancel(); }

boost::mysql::pooled_connection SqlPool::startTransaction() {
  auto conn = getConn();
  if (conn.valid()) {
    boost::mysql::results result;
    conn->execute("START TRANSACTION", result);
  } else {
    std::cerr << "Error: "
              << "Have not connected";
  }
  return conn;
}

void SqlPool::commit(boost::mysql::pooled_connection &conn) {
  if (conn.valid()) {
    boost::mysql::results result;
    conn->execute("COMMIT", result);
  }
}

SqlPool *SqlPool::instance() {
  static SqlPool pool;
  return &pool;
}

boost::mysql::pooled_connection SqlPool::getConn() {
  boost::mysql::diagnostics diag;
  boost::mysql::pooled_connection conn;
  try {
    auto future = m_pool->async_get_connection(std::chrono::seconds(3), diag,
                                               boost::asio::use_future);
    conn = std::move(future.get());
  } catch (const boost::mysql::error_with_diagnostics &err) {
    std::cout << "Uncaught exception: ", err.what(),
        "\nServer diagnostics: ", err.get_diagnostics().server_message().data();
  } catch (const boost::system::system_error &ec) {
    std::cout << "getConn:" << ec.what();
  }
  return conn;
}

} // namespace data_center
