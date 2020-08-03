#include "main.h"
#include "client_ws.hpp"
#include <future>
#include <queue>
#include <string>

using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;

/** \brief Класс для конфигурации вебсокет клиента
 */
class WsClientConfig {
public:
    std::string point;
    std::shared_ptr<WsClient::Connection> ws_connection;
    std::shared_ptr<WsClient> client;
    std::queue<std::string> queue_message;
    std::mutex queue_message_mutex;

    std::atomic<int> status = ATOMIC_VAR_INIT(0);
    std::atomic<bool> is_open = ATOMIC_VAR_INIT(false);
    std::atomic<bool> is_close = ATOMIC_VAR_INIT(false);
    std::atomic<bool> is_error = ATOMIC_VAR_INIT(false);

    HANDLE  handle_thread = NULL;
    DWORD   thread_id;

    inline bool check_running() {
        DWORD exit_code = 0;
        if(handle_thread) GetExitCodeThread(handle_thread, &exit_code);
        if(exit_code == STILL_ACTIVE) return true;

        return false;
    }

    WsClientConfig() {};

    ~WsClientConfig() {
        is_close = true;
        is_open = false;
        std::shared_ptr<WsClient> client_ptr = std::atomic_load(&client);
        if(client_ptr) {
            client_ptr->stop();
        }
        //if(check_running()) TerminateThread(handle_thread, 0);
        if(handle_thread) {
            WaitForSingleObject(handle_thread, INFINITE);
            CloseHandle(handle_thread);
        }
    };

    static DWORD WINAPI static_thread_start(void* param) {
        WsClientConfig* this_class = (WsClientConfig*) param;
        this_class->init();
        return 0;
    }

    void init() {
        try {
        //client_future = std::async(std::launch::async,[&]() {
            client = std::make_shared<WsClient>(point);

            client->on_message = [&](std::shared_ptr<WsClient::Connection> connection, std::shared_ptr<WsClient::InMessage> in_message) {
                std::lock_guard<std::mutex> lock(queue_message_mutex);
                queue_message.push(in_message->string());
            };

            client->on_open = [&](std::shared_ptr<WsClient::Connection> connection) {
                ws_connection = connection;
                status = 0;
                is_close = false;
                is_error = false;
                is_open = true;
            };

            client->on_close = [&](std::shared_ptr<WsClient::Connection> /*connection*/, int status, const std::string & /*reason*/) {
                status = status;
                is_close = true;
                is_open = false;
            };

            // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
            client->on_error = [&](std::shared_ptr<WsClient::Connection> /*connection*/, const SimpleWeb::error_code &ec) {
                is_close = true;
                is_error = true;
                is_open = false;
            };

            client->start();
            is_close = true;
            is_open = false;
        //});
        }
        catch(...) {};
    }

    /** \brief Получить сообщение
     *
     * Данный метод вернет пустую строку, если собщений нет. Или вернет первое сообщение из очереди
     * \return Сообщение из очереди или пустая строка
     */
    std::string get_message() {
        std::lock_guard<std::mutex> lock(queue_message_mutex);
        if(queue_message.size() == 0) return std::string();
        std::string message = queue_message.front();
        queue_message.pop();
        return message;
    }

    void start(const std::string &user_point) {
        point = user_point;
        handle_thread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)*static_thread_start,(void*)this, 0, &thread_id);
    }

    void send(const std::string &message) {
        if(ws_connection && is_open) ws_connection->send(message);
    }

    void close() {
        is_close = true;
        is_open = false;
        if(ws_connection && is_open) ws_connection->send_close(1000);
    }

    bool check_open() {
        return is_open;
    }

    bool check_close() {
        return is_close;
    }
};

enum {
    WS_CMD_OPEN = 1,
    WS_CMD_SEND = 2,
    WS_CMD_RECEIVE = 3,
    WS_CMD_CLOSE = 4,
};


static std::string ws_point;
static std::string ws_message;
static std::atomic<int> ws_handle = ATOMIC_VAR_INIT(0);
static std::atomic<int> ws_size = ATOMIC_VAR_INIT(0);
static std::atomic<int> ws_cmd = ATOMIC_VAR_INIT(0);
static std::atomic<bool> is_wait = ATOMIC_VAR_INIT(false); // переменая для ожидания обработки команды
//static std::atomic<bool> is_func_wait = ATOMIC_VAR_INIT(false); // переменая для ожидания обработки команды

static std::mutex ws_ptr_map_mutex;
static std::map<int, WsClientConfig*> ws_ptr_map;

static HANDLE  hThread = NULL; // основной поток
void WINAPI main_thread(void); // функция основного потока

int wait() {
    const int max_tick = 5000;
    int tick = 0;
    while(is_wait) {
        Sleep(1);
        ++tick;
        if(tick >= max_tick) return 0;
    }
    return 1;
}

int DLL_EXPORT ws_open(const char *point) {
    if(wait() == 0) {
        return 0;
    }
    ws_point = std::string(point);
    is_wait = true;
    ws_cmd = WS_CMD_OPEN;
    if(wait() == 0) {
        return 0;
    }
    return ws_handle;
}

int DLL_EXPORT ws_send(const int handle, const char *msg) {
    /* старая реализация
    if(wait() == 0) return 0;
    ws_message = std::string(msg);
    ws_handle = handle;
    is_wait = true;
    ws_cmd = WS_CMD_SEND;
    if(wait() == 0) return 0;
    return ws_size;
    */
    try {
        std::lock_guard<std::mutex> lock(ws_ptr_map_mutex);
        auto it = ws_ptr_map.find(handle);
        if(it == ws_ptr_map.end()) {
            return 0;
        } else
        if(it->second == nullptr) {
            return 0;
        } else
        if(!it->second->check_open()) {
            return 0;
        } else
        if(it->second->check_close()) {
            return 0;
        } else {
            std::string message(msg);
            if(it->second != nullptr) it->second->send(message);
            return message.size();
        }
    }
    catch(...) {}
    return 0;
}

int DLL_EXPORT ws_receive(const int handle, char *buffer, const int buffer_size) {
    /* старая реализация
    if(wait() == 0) return 0;
    ws_handle = handle;
    is_wait = true;
    ws_cmd = WS_CMD_RECEIVE;
    if(wait() == 0) return 0;
    try {
        if(ws_message.size() == 0) return 0;
        const int max_size = std::min((int)ws_message.size(), buffer_size);
        for(int i = 0; i < max_size; ++i) {
            buffer[i] = ws_message[i];
        }
        buffer[std::min((buffer_size - 1), max_size)] = '\0';
        return max_size;
    }
    catch(...) {}
    */
    try {
        std::string message;
        std::lock_guard<std::mutex> lock(ws_ptr_map_mutex);
        auto it = ws_ptr_map.find(handle);
        if(it == ws_ptr_map.end()) {
            message = std::string();
        } else {
            message = it->second->get_message();
        }
        if(message.size() == 0) return 0;
        const int max_size = std::min((int)message.size(), buffer_size);
        for(int i = 0; i < max_size; ++i) {
            buffer[i] = message[i];
        }
        buffer[std::min((buffer_size - 1), max_size)] = '\0';
        return max_size;
    }
    catch(...) {}
    return 0;
}

void DLL_EXPORT ws_close(const int handle) {
    if(wait() == 0) return;
    ws_handle = handle;
    is_wait = true;
    ws_cmd = WS_CMD_CLOSE;
    wait();
}

extern "C" DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DWORD dwThreadId;
            hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)*main_thread,0,0,&dwThreadId);
            if(hThread == NULL) return FALSE;
            break;

        case DLL_PROCESS_DETACH:
            TerminateThread(hThread, 0);
            break;

        case DLL_THREAD_ATTACH:
            // attach to thread
            break;

        case DLL_THREAD_DETACH:
            // detach from thread
            break;
    }
    return TRUE; // succesful
}

/* основной поток */
void WINAPI main_thread(void) {
    std::map<int, WsClientConfig> ws_client_map;
    int ws_handle_index = 1;
    while(true) {
        int new_cmd = ws_cmd;
        switch(new_cmd) {
        case WS_CMD_OPEN: {
                std::string point = ws_point;
                try {
                    /* блокируем функцию ws_open на весь период */
                    if(ws_client_map.find(ws_handle_index) == ws_client_map.end()) {
                        ws_client_map[ws_handle_index].start(std::string(point));
                    } else {
                        //auto it = ws_client_map.begin();
                        for(auto &it : ws_client_map) {
                            if(it.second.check_close()) {
                                ws_client_map.erase(it.first);
                            }
                        }

                        //if(ws_client_map[ws_handle_index].check_close()) {
                        //    ws_client_map.erase();
                        //}

                        ws_handle_index++;
                        ws_client_map[ws_handle_index].start(std::string(point));
                    }
                    while(true) {
                        if(ws_client_map[ws_handle_index].check_open()) {
                            /* удачное завершение дел, возвращаем handle */
                            ws_handle = ws_handle_index;
                            std::lock_guard<std::mutex> lock(ws_ptr_map_mutex);
                            ws_ptr_map[ws_handle_index] = (WsClientConfig*)&ws_client_map[ws_handle_index];
                            break;
                        }
                        if(ws_client_map[ws_handle_index].check_close()) {
                            /* ошибка соединения или оно было закрыто, выходим */
                            ws_handle = 0;
                            break;
                        }
                    }
                }
                catch(...) {}
            }
            is_wait = false;
            break;
        case WS_CMD_SEND: {
                int handle = ws_handle;
                try {
                    auto it = ws_client_map.find(handle);
                    if(it == ws_client_map.end()) {
                        ws_size = 0;
                    } else
                    if(!it->second.check_open()) {
                        ws_size = 0;
                    } else
                    if(it->second.check_close()) {
                        ws_size = 0;
                    } else {
                        it->second.send(ws_message);
                        ws_size = ws_message.size();
                    }
                }
                catch(...) {}
            }
            is_wait = false;
            break;
        case WS_CMD_RECEIVE: {
                int handle = ws_handle;
                try {
                    auto it = ws_client_map.find(handle);
                    if(it == ws_client_map.end()) {
                        ws_message = std::string();
                    } else {
                        ws_message = it->second.get_message();
                    }
                }
                catch(...) {}
            }
            is_wait = false;
            ws_cmd = 0;
            break;
        case WS_CMD_CLOSE: {
                int handle = ws_handle;
                try {
                    auto it = ws_client_map.find(handle);
                    if(it != ws_client_map.end()) {
                        it->second.close();
                    }
                }
                catch(...) {}
            }
            is_wait = false;
            break;
        };
        if(ws_cmd != 0) ws_cmd = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        /*
        WsClientConfig ws_client;
        ws_client.start("localhost:8080/echo");
        while(true) {
            if(ws_client.check_open()) break;
            if(ws_client.check_close()) break;
        }
        while(ws_client.check_open()) {
            ws_client.send("test!");
            Sleep(1000);
        }
        */
    }
    return;
}

