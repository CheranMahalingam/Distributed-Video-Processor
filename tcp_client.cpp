#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);

        socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 1300));

        for (;;)
        {
            boost::array<char, 1024> buf;
            boost::system::error_code error;

            size_t len = socket.read_some(boost::asio::buffer(buf), error);
            if (error == boost::asio::error::eof)
                break;
            else if (error)
                throw boost::system::system_error(error);
            
            std::cout.write(buf.data(), len);
        }
    } catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}