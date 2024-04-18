#include "SqlPool.hpp"

constexpr std::uint8_t POOL_SIZE = 10;

SqlPool::SqlPool(/* args */) {
  // TODO

  // Configuration for the connection pool
  // boost::mysql::pool_params pool_prms{
  //     // Connect using TCP, to the given hostname and using the default port
  //     boost::mysql::host_and_port{"192.168.1.158"},
  //     "gauture",
  //     "Gz-123456",
  //     "E600",
  // };
  // boost::mysql::connection_pool
  // p(boost::mysql::pool_executor_params::thread_safe(m_th_pool.get_executor()),
  //     std::move(pool_prms));
  // Create the connection pool
  // m_pool = boost::shared_ptr<boost::mysql::connection_pool>(new
  // boost::mysql::connection_pool(
  //     boost::mysql::pool_executor_params::thread_safe(m_th_pool.get_executor()),
  //     std::move(pool_prms)));
  // m_pool->async_run(boost::asio::detached);
  // std::cout << m_pool->valid() << std::endl;
}

SqlPool::~SqlPool() {
  while (!m_cons.empty()) {
    try {
      m_cons.front();
      m_cons.pop();
    } catch (const boost::mysql::error_with_diagnostics &err) {
      std::cerr << "Error: " << err.what() << '\n'
                << "Server diagnostics: "
                << err.get_diagnostics().server_message() << std::endl;
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
    }
  }
}

void SqlPool::connect(std::string ip, std::uint32_t port, std::string username,
                      std::string password, std::string database) {
  try {
    for (int i = 0; i < POOL_SIZE; i++) {
      m_cons.push(
          std::make_shared<connode>(ip, port, username, password, database));
      std::cout << "connect success" << std::endl;
    }
  } catch (const boost::mysql::error_with_diagnostics &err) {
    std::cerr << "Error: " << err.what() << '\n'
              << "Server diagnostics: "
              << err.get_diagnostics().server_message() << std::endl;
    exit(0);
  } catch (const std::exception &err) {
    std::cerr << "Error: " << err.what() << std::endl;
    exit(0);
  }
}
std::shared_ptr<connode> SqlPool::getCon() {
  std::unique_lock<std::mutex> lk(m_mtx);
  if (!m_cons.empty()) {
    auto conn = m_cons.front();
    m_cons.pop();
    return conn;
  }
  std::cerr << "Error: " << "Have not connected" << std::endl;
  return {};
}

void SqlPool::releaseCon(std::shared_ptr<connode> con) {
  std::unique_lock<std::mutex> lk(m_mtx);
  m_cons.push(con);
}
