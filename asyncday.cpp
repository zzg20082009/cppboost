#include <ctime>
#include <string>
#include <iostream>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string make_daytime() {
  time_t now = time(0);
  return ctime(&now);
}

class tcp_connection : std::enable_shared_from_this<tcp_connection> {
public:
  typedef std::shared_ptr<tcp_connection> pointer;
  static pointer create(boost::asio::io_context& io) {
    return pointer(new tcp_connection(io));
  }
  tcp::socket& socket() {
    return socket_;
  }
  void start() {
    message_ = make_daytime();
    boost::asio::async_write(socket_, boost::asio::buffer(message_),std::bind(&tcp_connection::handle_write, shared_from_this(),
										 std::placeholders::_1,
										 std::placeholders::_2));
  }
private:
  tcp::socket socket_;
  std::string message_;     // 因为是异步写，所以需要保存message_的内容.
  tcp_connection(boost::asio::io_context& io) : socket_(io) { }
  void handle_write(const boost::system::error_code&, size_t) { }
};

class tcp_server
{
private:
  boost::asio::io_context& io_;   // 因为要创建新连接(tcp_connection)，新的连接要在这个context中，所以需要保存
  tcp::acceptor acceptor_;
public:
  tcp_server(boost::asio::io_context& io) :
    io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), 13)) {
    start_accept();
  }
private:
  void start_accept() {
    tcp_connection::pointer new_connection = tcp_connection::create(io_);  // 创建的是共享指针，因为下一条操作（async_accept)是异步的，所以当连接过来时，
    acceptor_.async_accept(new_connection->socket(), std::bind(&tcp_server::handle_accept, this,
								 new_connection, std::placeholders::_1)); // 有可能start_accept已退出，保存连接的栈退出连接信息不存在了
    // 当有客户请求到来时，会通知操作系统调用handle_accept函数，由于是异步的等待请求到来，所以，当请求真正到来时，这个start_accept的执行线程可能已经退出，如果start_accept退出，如果没有
    // 共享指针做保证的话，new_connection对象可能就已经析构，不存在了。所以需要使用共享指针来保证，当回执行异步调用时，对应的回调函数及其栈帧被建立，但不会被调用，直到条件发生，激发该函数
  }
  void handle_accept(tcp_connection::pointer new_connection, const boost::system::error_code& error) {
    // if (!error) {
      new_connection->start();
      //}
    start_accept();
  }
};
    
int main()
{
  try {
    boost::asio::io_context io;
    tcp_server server(io);
    io.run();
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
