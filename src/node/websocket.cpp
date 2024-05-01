#include <cassert>
#include <functional>
#include <iostream>

#include <emscripten/websocket.h>
#include <iostream>
#include <span>
#include <thread>
using namespace std;

class EmscriptenWSConnection {
  static void assert_success(EMSCRIPTEN_RESULT r) {
    assert(r == EMSCRIPTEN_RESULT_SUCCESS);
  }
  template <typename T>
  auto get_socket_val(EMSCRIPTEN_RESULT (*fun)(EMSCRIPTEN_WEBSOCKET_T,
                                               T *)) const {
    T val;
    assert_success(fun(socket, &val));
    return val;
  }
  std::string get_socket_string(
      EMSCRIPTEN_RESULT (*length_fun)(EMSCRIPTEN_WEBSOCKET_T, int *),
      EMSCRIPTEN_RESULT (*fun)(EMSCRIPTEN_WEBSOCKET_T, char *, int)) const {
    std::string result;
    result.resize(get_socket_val(length_fun));
    assert_success(fun(socket, result.data(), result.length()));
    return result;
  }

public:
  using open_callback_t =
      std::function<bool(EmscriptenWSConnection &, const EmscriptenWebSocketOpenEvent &)>;
  using close_callback_t =
      std::function<bool(EmscriptenWSConnection &, const EmscriptenWebSocketCloseEvent &)>;
  using error_callback_t =
      std::function<bool(EmscriptenWSConnection &, const EmscriptenWebSocketErrorEvent &)>;
  using message_callback_t =
      std::function<bool(EmscriptenWSConnection &, const EmscriptenWebSocketMessageEvent &)>;
  struct Callbacks {
    open_callback_t open_callback;
    close_callback_t close_callback;
    error_callback_t error_callback;
    message_callback_t message_callback;
  };
  static bool is_supported() { return emscripten_websocket_is_supported(); }
  enum class ReadyState { CONNECTING = 0, OPEN = 1, CLOSING = 2, CLOSED = 3 };
  ReadyState get_ready_state() const {
    auto readyState = get_socket_val(emscripten_websocket_get_ready_state);
    assert(readyState >= 0 && readyState <= 3);
    return static_cast<ReadyState>(readyState);
  }
  auto get_buffered_amount() const {
    return get_socket_val(emscripten_websocket_get_buffered_amount);
  }
  auto url_length() const {
    return get_socket_val(emscripten_websocket_get_url_length);
  }
  std::string get_url() const {
    return get_socket_string(emscripten_websocket_get_url_length,
                             emscripten_websocket_get_url);
  }
  std::string get_extensions() const {
    return get_socket_string(emscripten_websocket_get_extensions_length,
                             emscripten_websocket_get_extensions);
  }
  std::string get_protocol() const {
    return get_socket_string(emscripten_websocket_get_protocol_length,
                             emscripten_websocket_get_protocol);
  }
  EMSCRIPTEN_RESULT send_binary(std::span<uint8_t> s) {
    return emscripten_websocket_send_binary(socket, s.data(), s.size());
  }
  EMSCRIPTEN_RESULT send_utf8_text(std::string s) {
    return emscripten_websocket_send_utf8_text(socket, s.c_str());
  }
  EMSCRIPTEN_RESULT close(unsigned short code = 1000, const char *reason = "") {
    return emscripten_websocket_close(socket, code, reason);
  }

  EmscriptenWSConnection(std::string url, Callbacks callbacks) : callbacks(callbacks) {
    EmscriptenWebSocketCreateAttributes createAttributes{
        .url = url.c_str(), .protocols = "binary", .createOnMainThread = true};
    socket = emscripten_websocket_new(&createAttributes);
    if (socket <= 0) {
      switch (socket) {
      case EMSCRIPTEN_RESULT_NOT_SUPPORTED:
        throw std::runtime_error(
            "Cannot connect: " +
            std::string("EMSCRIPTEN_RESULT_NOT_SUPPORTED"));
      case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED:
        throw std::runtime_error(
            "Cannot connect: " +
            std::string("EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED"));
      case EMSCRIPTEN_RESULT_INVALID_TARGET:
        throw std::runtime_error(
            "Cannot connect: " +
            std::string("EMSCRIPTEN_RESULT_INVALID_TARGET"));
      case EMSCRIPTEN_RESULT_UNKNOWN_TARGET:
        throw std::runtime_error(
            "Cannot connect: " +
            std::string("EMSCRIPTEN_RESULT_UNKNOWN_TARGET"));
      case EMSCRIPTEN_RESULT_INVALID_PARAM:
        throw std::runtime_error(
            "Cannot connect: " +
            std::string("EMSCRIPTEN_RESULT_INVALID_PARAM"));
      case EMSCRIPTEN_RESULT_FAILED:
        throw std::runtime_error("Cannot connect: " +
                                 std::string("EMSCRIPTEN_RESULT_FAILED"));
      case EMSCRIPTEN_RESULT_NO_DATA:
        throw std::runtime_error("Cannot connect: " +
                                 std::string("EMSCRIPTEN_RESULT_NO_DATA"));
      case EMSCRIPTEN_RESULT_TIMED_OUT:
        throw std::runtime_error("Cannot connect: " +
                                 std::string("EMSCRIPTEN_RESULT_TIMED_OUT"));
      default:
        throw std::runtime_error("Cannot connect: " +
                                 std::string("Unknown reason"));
      }
    }
    cout << "Set open" << endl;
    assert_success(emscripten_websocket_set_onopen_callback_on_thread(
        socket, this,
        [](int /*eventType*/, const auto *e, void *user_data) -> EM_BOOL {
          cout << "on_open" << endl;
          auto &ws{*static_cast<EmscriptenWSConnection *>(user_data)};
          auto cb{ws.callbacks.open_callback};
          if (cb)
            return cb(ws, *e);
          return 0;
        },
        EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD));
    assert_success(emscripten_websocket_set_onclose_callback_on_thread(
        socket, this,
        [](int /*eventType*/, const auto *e, void *user_data) -> EM_BOOL {
          cout << "on_close" << endl;
          auto &ws{*static_cast<EmscriptenWSConnection *>(user_data)};
          auto cb{ws.callbacks.close_callback};
          if (cb)
            return cb(ws, *e);
          return 0;
        },
        EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD));
    assert_success(emscripten_websocket_set_onerror_callback_on_thread(
        socket, this,
        [](int /*eventType*/, const auto *e, void *user_data) -> EM_BOOL {
          cout << "on_error" << endl;
          auto &ws{*static_cast<EmscriptenWSConnection *>(user_data)};
          auto cb{ws.callbacks.error_callback};
          if (cb)
            return cb(ws, *e);
          return 0;
        },
        EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD));
    assert_success(emscripten_websocket_set_onmessage_callback_on_thread(
        socket, this,
        [](int /*eventType*/, const EmscriptenWebSocketMessageEvent *e,
           void *user_data) -> EM_BOOL {
          cout << "on_message is_text:" << e->isText
               << " length: " << e->numBytes << endl;

          if (e->isText)
            cout << std::string((char *)e->data, e->numBytes) << endl;
          auto &ws{*static_cast<EmscriptenWSConnection *>(user_data)};
          auto cb{ws.callbacks.message_callback};
          if (cb)
            return cb(ws, *e);
          return 0;
        },
        EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD));
  }
  ~EmscriptenWSConnection() { assert_success(emscripten_websocket_delete(socket)); }

private:
  Callbacks callbacks;
  EMSCRIPTEN_WEBSOCKET_T socket;
};

//
//
// // Closes the specified WebSocket. N.B.: the meaning of "closing" a WebSocket
// // means "eager read/lazy write"-closing the socket. That is, all still
// pending
// // untransferred outbound bytes will continue to transfer out, but after
// calling
// // close on the socket, any pending bytes still in the process of being
// received
// // will never be available. See
// //
// https://html.spec.whatwg.org/multipage/web-sockets.html#dom-websocket-sclose
// // After calling close(), it is no longer possible to send() on the WebSocket
// to
// // send more bytes.
// EMSCRIPTEN_RESULT emscripten_websocket_close(EMSCRIPTEN_WEBSOCKET_T socket,
//                                              unsigned short code,
//                                              const char *reason);
//
// } // ~extern "C"

#include <emscripten.h>
//
using namespace std;
int main() {
  cout << "S" << endl;
  EmscriptenWSConnection::Callbacks callbacks{
      .open_callback = [](EmscriptenWSConnection &ws,
                          const EmscriptenWebSocketOpenEvent &) -> bool {
          cout<<"Send: "<< ws.send_utf8_text(std::string("Sa").c_str())<<endl;
          cout<<"Status "<<static_cast<int>(ws.get_ready_state())<<endl;
          
          cout<<"Status "<<static_cast<int>(ws.get_ready_state())<<endl;
          cout<<"Thread id "<<std::this_thread::get_id()<<endl;
        return true;
      }};
  // EmscriptenWSConnection ws("wss://echo.websocket.org/", callbacks);
  EmscriptenWSConnection ws("ws://127.0.0.1:10001/", callbacks);
  cout << "A" << endl;
  this_thread::sleep_for(chrono::seconds(2));
  cout<<"Send: "<< ws.send_utf8_text(std::string("Sa").c_str())<<endl;
  cout<<"Status "<<static_cast<int>(ws.get_ready_state())<<endl;
  cout<<"Thread id "<<std::this_thread::get_id()<<endl;
  ws.close();
  cout<<"Send: "<< ws.send_utf8_text(std::string("Sa").c_str())<<endl;
  cout<<"Status "<<static_cast<int>(ws.get_ready_state())<<endl;
  this_thread::sleep_for(chrono::seconds(5));
  cout.flush();
  // w.set_on_open(
  return 0;
}
