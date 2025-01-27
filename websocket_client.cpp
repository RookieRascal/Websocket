// websocket_client.cpp
#include "websocket_client.h"
#include <iostream>

std::shared_ptr<WebSocketClient> WebSocketClient::create(
    net::io_context& ioc,
    ssl::context& ssl_ctx
) {
    return std::shared_ptr<WebSocketClient>(new WebSocketClient(ioc, ssl_ctx));
}

WebSocketClient::WebSocketClient(net::io_context& ioc, ssl::context& ssl_ctx)
    : resolver_(ioc)
    , ws_(ioc, ssl_ctx)
{
    message_callback_ = [](const std::string& msg) {
        std::cout << "Received: " << msg << std::endl;
    };
    close_callback_ = [](const std::string& reason) {
        std::cout << "Connection closed: " << reason << std::endl;
    };
}

void WebSocketClient::connect(
    const std::string& host,
    const std::string& port,
    const std::string& path,
    ConnectCallback callback
) {
    host_ = host;

    if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()),
            boost::asio::error::get_ssl_category()};
        callback(false, ec.message());
        return;
    }

    ws_.next_layer().next_layer().expires_after(std::chrono::seconds(30));

    resolver_.async_resolve(
        host,
        port,
        beast::bind_front_handler(
            &WebSocketClient::on_resolve,
            shared_from_this(),
            std::move(callback)
        )
    );
}

void WebSocketClient::on_resolve(
    const boost::system::error_code& ec,
    tcp::resolver::results_type results,
    ConnectCallback callback
) {
    if(ec) {
        callback(false, ec.message());
        return;
    }

    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(
            &WebSocketClient::on_connect,
            shared_from_this(),
            std::move(callback)
        )
    );
}

void WebSocketClient::on_connect(
    const boost::system::error_code& ec,
    tcp::resolver::results_type::endpoint_type ep,
    ConnectCallback callback
) {
    if(ec) {
        callback(false, ec.message());
        return;
    }

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(
            &WebSocketClient::on_ssl_handshake,
            shared_from_this(),
            std::move(callback)
        )
    );
}

void WebSocketClient::on_ssl_handshake(
    const boost::system::error_code& ec,
    ConnectCallback callback
) {
    if(ec) {
        callback(false, ec.message());
        return;
    }

    beast::get_lowest_layer(ws_).expires_never();

    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::client
        )
    );

    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                " websocket-client");
        }
    ));

    ws_.async_handshake(
        host_,
        "/",
        beast::bind_front_handler(
            &WebSocketClient::on_handshake,
            shared_from_this(),
            std::move(callback)
        )
    );
}

void WebSocketClient::on_handshake(
    const boost::system::error_code& ec,
    ConnectCallback callback
) {
    if(ec) {
        callback(false, ec.message());
        return;
    }

    connected_ = true;
    callback(true, "");
    do_read();
}

void WebSocketClient::do_read() {
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &WebSocketClient::on_read,
            shared_from_this()
        )
    );
}

void WebSocketClient::on_read(
    const boost::system::error_code& ec,
    std::size_t bytes_transferred
) {
    if(ec == websocket::error::closed) {
        connected_ = false;
        close_callback_("Connection closed by peer");
        return;
    }

    if(ec) {
        connected_ = false;
        close_callback_("Error: " + ec.message());
        return;
    }

    message_callback_(beast::buffers_to_string(buffer_.data()));
    buffer_.consume(buffer_.size());
    do_read();
}

void WebSocketClient::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.push(message);

    if(!writing_) {
        do_write();
    }
}

void WebSocketClient::do_write() {
    std::lock_guard<std::mutex> lock(write_mutex_);

    if(write_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;

    ws_.async_write(
        net::buffer(write_queue_.front()),
        beast::bind_front_handler(
            &WebSocketClient::on_write,
            shared_from_this()
        )
    );
}

void WebSocketClient::on_write(
    const boost::system::error_code& ec,
    std::size_t bytes_transferred
) {
    if(ec) {
        connected_ = false;
        close_callback_("Write error: " + ec.message());
        return;
    }

    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.pop();
    do_write();
}

void WebSocketClient::close(websocket::close_code code) {
    if(!connected_) return;

    boost::system::error_code ec;
    ws_.close(code, ec);
    
    if(ec) {
        close_callback_("Close error: " + ec.message());
    }
    
    connected_ = false;
}
