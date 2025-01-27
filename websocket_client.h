// websocket_client.h
#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
#include <memory>
#include <string>
#include <queue>
#include <mutex>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
public:
    using ConnectCallback = std::function<void(bool success, const std::string& error)>;
    using MessageCallback = std::function<void(const std::string& message)>;
    using CloseCallback = std::function<void(const std::string& reason)>;

    static std::shared_ptr<WebSocketClient> create(
        net::io_context& ioc,
        ssl::context& ssl_ctx
    );

    void connect(
        const std::string& host,
        const std::string& port,
        const std::string& path,
        ConnectCallback callback
    );

    void send(const std::string& message);
    void close(websocket::close_code code = websocket::close_code::normal);

    void set_message_callback(MessageCallback callback) { message_callback_ = std::move(callback); }
    void set_close_callback(CloseCallback callback) { close_callback_ = std::move(callback); }

    bool is_connected() const { return connected_; }

private:
    WebSocketClient(net::io_context& ioc, ssl::context& ssl_ctx);
    
    void on_resolve(
        const boost::system::error_code& ec,
        tcp::resolver::results_type results,
        ConnectCallback callback
    );

    void on_connect(
        const boost::system::error_code& ec,
        tcp::resolver::results_type::endpoint_type ep,
        ConnectCallback callback
    );

    void on_ssl_handshake(
        const boost::system::error_code& ec,
        ConnectCallback callback
    );

    void on_handshake(
        const boost::system::error_code& ec,
        ConnectCallback callback
    );

    void do_read();
    void on_read(
        const boost::system::error_code& ec,
        std::size_t bytes_transferred
    );

    void do_write();
    void on_write(
        const boost::system::error_code& ec,
        std::size_t bytes_transferred
    );

    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::queue<std::string> write_queue_;
    std::mutex write_mutex_;
    bool writing_{false};
    bool connected_{false};
    MessageCallback message_callback_;
    CloseCallback close_callback_;
};

#endif // WEBSOCKET_CLIENT_H
