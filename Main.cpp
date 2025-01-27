// main.cpp
#include "websocket_client.h"
#include <iostream>
#include <thread>

int main() {
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        auto client = WebSocketClient::create(ioc, ctx);

        client->set_message_callback([](const std::string& msg) {
            std::cout << "Received: " << msg << std::endl;
        });

        client->set_close_callback([](const std::string& reason) {
            std::cout << "Connection closed: " << reason << std::endl;
        });

        client->connect("echo.websocket.org", "443", "/", 
            [client](bool success, const std::string& error) {
                if(!success) {
                    std::cout << "Connection failed: " << error << std::endl;
                    return;
                }

                std::cout << "Connected successfully!" << std::endl;
                client->send("Hello, secure WebSocket!");
            });

        ioc.run();
    }
    catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
