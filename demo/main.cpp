
#include <thread>
#include "SqlPool.hpp"
#include <boost/describe.hpp>



// 反射 [对应到 table 的字段
// （必须与表字段相同，估计用了这个属性字符串来取值赋值]
struct usernode {
  std::int64_t id;

  boost::optional<std::string> username; // 根据数据库字段属性，没有强制必须有值

  boost::optional<std::string> password;

  boost::mysql::datetime createtime;

  boost::optional<boost::mysql::datetime> logintime;
};
BOOST_DESCRIBE_STRUCT(usernode, (),
                      (id, username, password, createtime, logintime))

struct parnode
{
    std::string logouttime;
};

BOOST_DESCRIBE_STRUCT(parnode, (),(logouttime))

struct usernodes {
  std::vector<usernode> nodes;
};
BOOST_DESCRIBE_STRUCT(usernodes, (), (nodes))

int main(int argc, char **argv) {
    data_center::SqlPool* pool=data_center::SqlPool::instance();

    std::thread th([&]() {
        boost::mysql::results res;
        pool->query("SELECT * from user ", res);
        if (res.has_value()) {
            for (boost::mysql::row_view item : res.rows()) {
                std::cout << "* ID: " << item.at(0) << '\n'
                          << "  User name: " << item.at(1) << '\n'
                          << "  create time: " << item.at(2) << '\n'
                          << "  login time: " << item.at(3) << "$" << std::endl;
            }
        }
    });

    // 绑定到结构体
    {
        boost::mysql::static_results<usernode> result;
        pool->query("SELECT id, username,password,createtime,logintime from user "
                    "where username=?",
                    result, "admin");
        if (result.has_value()) {
            std::vector<usernode> data = std::move(
                std::vector<usernode>(std::make_move_iterator(result.rows().begin()),
                                      std::make_move_iterator(result.rows().end())));

            // 取一行读取
            usernode node = std::move(result.rows()[0]);

            auto &[id, username, password, createtime, logintime] = node;

            std::cout << "  id: " << id << '\n'
                      << "  User name: " << username.value_or("").c_str() << '\n'
                      << "  password: " << password.value_or("").c_str() << '\n'
                      << "  createtime: " << datateTostring(createtime) << '\n'
                      << "  logintime: " << datateTostring(logintime.value()) << '\n'
                      << "$" << std::endl;

            boost::mp11::mp_for_each<boost::describe::describe_members<usernode, boost::describe::mod_any_access>>(
                [&](auto D) {
                    std::cout << D.name << ": " << node.*D.pointer << std::endl;
                });
           auto restp = struct_to_tuple(node);
           // auto& [a,b,c,d] =restp;
           std::cout << "  id: "<<std::get<0>(restp)<<std::endl;
        }
    }

    // 利用一个tuple查询
    {
        std::array<std::string, 3> arry{"chpan", "123", "2024-04-03 08:37:20"};
        std::tuple<std::string, std::string, std::string> tp =
            std::make_tuple("chpan", "123", "2024-04-03 08:37:20");
        boost::mysql::results result;
        pool->query(
            "INSERT INTO user(username,password,createtime) values(?,?,?)", result,
            tp);
        if (result.has_value()) {
            if (result.affected_rows() != 0) {
                std::cout << "INSERT success" << std::endl;
            }
        }
    }

    {
        boost::mysql::results result;
        pool->query("DELETE from user where username=?", result,
                           std::make_tuple("chpan"));
        if (result.has_value()) {
            if (result.affected_rows() != 0) {
                std::cout << "DELETE success" << std::endl;
            }
        }
    }

    // 多种参数复合封装成  查询
    {
        std::array<std::string, 1> arry{"nihao"};
        parnode structlogtime;
        structlogtime.logouttime="2024-04-22 11:37:20";
        boost::mysql::results result;
        pool->query("INSERT INTO user(logintime,username,password,createtime,logouttime) "
                    "values(?,?,?,?,?)",
                    result, "2024-04-03 08:37:20",std::make_tuple("123"),arry,"2024-04-03 08:37:20",struct_to_tuple(structlogtime));
        if (result.has_value()) {
            if (result.affected_rows() != 0) {
                std::cout << "INSERT success" << std::endl;
            }
        }
    }

    {
        boost::mysql::results result;
        pool->query("DELETE from user where username=?", result, "123");
        if (result.has_value()) {
            if (result.affected_rows() != 0) {
                std::cout << "DELETE success" << std::endl;
            }
        }
    }



    th.join();
}
