#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string make_daytime_string()
{
    std::time_t now = std::time(0);
    return std::ctime(&now);
}

class tcp_connection
    : public boost::enable_shared_from_this<tcp_connection>
{
public:
    typedef boost::shared_ptr<tcp_connection> pointer;

    static pointer create(boost::asio::io_context& io_context)
    {
        std::cout << "Connection Created" << std::endl;
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket()
    {
        return socket_;
    }

    void start()
    {
        message_ = make_daytime_string();

        boost::asio::async_read(socket_, boost::asio::buffer(data, max_length),
            boost::bind(&tcp_connection::handle_read, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));

        boost::asio::async_write(socket_, boost::asio::buffer(message_),
            boost::bind(&tcp_connection::handle_write, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

private:
    tcp_connection(boost::asio::io_context& io_context)
        : socket_(io_context) {}

    void handle_write(const boost::system::error_code& err, size_t bytes_transferred)
    {
        if (!err) {
            std::cout << "Message Sent" << std::endl;
        } else {
            std::cerr << "error: " << err.message() << std::endl;
            socket_.close();
        }
    }

    void handle_read(const boost::system::error_code& err, size_t bytes_transferred)
    {
        if (!err) {
            std::cout << data << std::endl;
        } else {
            std::cerr << "error: " << err.message() << std::endl;
            socket_.close();
        }
    }

    tcp::socket socket_;
    std::string message_;
    enum { max_length = 1024 };
    char data[max_length];
};

class tcp_server
{
public:
    tcp_server(boost::asio::io_context& io_context)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 1300)),
          connections_({})
    {
        start_accept();
    }

private:
    void start_accept()
    {
        tcp_connection::pointer new_connection = tcp_connection::create(io_context_);
        acceptor_.async_accept(new_connection->socket(),
            boost::bind(&tcp_server::handle_accept, this, new_connection,
                boost::asio::placeholders::error));
    }

    void handle_accept(tcp_connection::pointer new_connection, const boost::system::error_code& error)
    {
        if (!error)
        {
            std::cout << "Connection Accepted" << std::endl;
            connections_.push_back(new_connection);
            new_connection->start();
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::vector<tcp_connection::pointer> connections_;
};

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp_server server(io_context);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}