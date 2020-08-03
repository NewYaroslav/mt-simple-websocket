#include <iostream>
#include "server_ws.hpp"
#include <future>

using namespace std;
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;

int main() {
    // WebSocket (WS)-server at port 8080 using 1 thread
    WsServer server;
    server.config.port = 8080;
    int send_index = 0;

    // Example 1: echo WebSocket endpoint
    // Added debug messages for example use of the callbacks
    // Test with the following JavaScript:
    //   var ws=new WebSocket("ws://localhost:8080/echo");
    //   ws.onmessage=function(evt){console.log(evt.data);};
    //   ws.send("test");
    auto &echo = server.endpoint["^/echo/?$"];

    echo.on_message = [&](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> in_message) {
        auto out_message = in_message->string();

        std::cout << "Server: Message received: \"" << out_message << "\" from " << connection.get() << std::endl;



        std::string server_out_message("Hi! I im server! " + std::to_string(send_index));
        ++send_index;
        std::cout << "Server: Sending message \"" << server_out_message << "\" to " << connection.get() << std::endl;

        // connection->send is an asynchronous function
        connection->send(server_out_message, [=](const SimpleWeb::error_code &ec) {
            if(ec) {
                std::cout << "Server: Error sending message. " <<
                // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                "Error: " << ec << ", error message: " << ec.message() << endl;
            } else {

            }
        });

        // Alternatively use streams:
        // auto out_message = make_shared<WsServer::OutMessage>();
        // *out_message << in_message->string();
        // connection->send(out_message);
    };

    echo.on_open = [](std::shared_ptr<WsServer::Connection> connection) {
        std::cout << "Server: Opened connection " << connection.get() << endl;
    };

    // Смотрите RFC 6455 7.4.1. для кодов состояния
    echo.on_close = [](std::shared_ptr<WsServer::Connection> connection, int status, const string & /*reason*/) {
        std::cout << "Server: Closed connection " << connection.get() << " with status code " << status << endl;
    };

    // При необходимости можно изменить заголовки ответа на рукопожатие.
    echo.on_handshake = [](std::shared_ptr<WsServer::Connection> /*connection*/, SimpleWeb::CaseInsensitiveMultimap & /*response_header*/) {
        return SimpleWeb::StatusCode::information_switching_protocols; // Обновление до веб-сокета
    };

    // Смотрите http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Коды ошибок для значений кодов ошибок
    echo.on_error = [](shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
        std::cout << "Server: Error in connection " << connection.get() << ". "
            << "Error: " << ec << ", error message: " << ec.message() <<std:: endl;
    };

    // Запустите сервер и получите назначенный порт, когда сервер прослушивает запросы
    std::promise<unsigned short> server_port;
    std::thread server_thread([&server, &server_port]() {
        // Запустить сервер
        server.start([&server_port](unsigned short port) {
            server_port.set_value(port);
        });
    });
    std::cout << "Server listening on port " << server_port.get_future().get() << std::endl << std::endl;
    server_thread.join();
    return 0;
}
