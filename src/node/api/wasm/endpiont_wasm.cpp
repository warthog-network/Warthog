#include "endpiont_wasm.hpp"
#include "../hook_endpoints.hxx"
#include "api/events/emit.hpp"
#include "api/wasm/api.hpp"
#include "router.hpp"
class WasmEndpoint {
    // dummy for wasm case
    class IndexGenerator {
    public:
        void get(std::string) {};
        void post(std::string) {};
        void section(std::string) {};
    };
    template <typename T>
    friend class RouterHook;
    // router
    using Router = router::Router;
    using Response = router::Response;

    static constexpr bool isPublic { true };
    IndexGenerator indexGenerator;
    Router r;
    Router& router() { return r; }
    static void async_reply(Response* res, std::string reply)
    {
        api::emit_api_result(res->id, std::move(reply));
    };
    static void reply_json(Response* res, std::string reply)
    {
        api::emit_api_result(res->id, std::move(reply));
    }
    static void insert_pending(Response*) { } // dummy
public:
    WasmEndpoint()
    {
        hook_endpoints(*this);
    }
    bool get(size_t id, std::string_view url)
    {
        return router().match_get(id, url);
    }
    bool post(size_t id, std::string_view url, std::string_view postData)
    {
        return router().match_post(id, url, postData);
    }
};

struct SubscriptionData {
};

namespace {
WasmEndpoint endpoint;
std::mutex m;
bool useBuffer { true };
struct BufferedRequest {
    struct Get {
        int id;
        std::string path;
    };
    struct Post {
        int id;
        std::string path;
        std::string postdata;
    };
    using variant_t = std::variant<Get, Post>;
    variant_t variant;
};
std::vector<BufferedRequest> requestBuffer;
}

void handle_post(router::id_t id, std::string_view path, std::string_view postdata)
{
    if (!endpoint.post(id, path, postdata))
        api::emit_api_result(id, jsonmsg::status(Error(ENOTFOUND)));
}
void handle_get(router::id_t id, std::string_view path)
{
    if (!endpoint.get(id, path))
        api::emit_api_result(id, jsonmsg::status(Error(ENOTFOUND)));
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
void virtual_get_request(router::id_t id, char* path)
{
    std::unique_lock l(m);
    if (useBuffer) {
        l.unlock();
        requestBuffer.push_back({ BufferedRequest::Get { id, path } });
    } else {
        handle_get(id, path);
    }
}
EMSCRIPTEN_KEEPALIVE
void virtual_post_request(router::id_t id, char* path, char* postdata)
{
    std::unique_lock l(m);
    if (useBuffer) {
        l.unlock();
        requestBuffer.push_back({ BufferedRequest::Post { id, path, postdata } });
    } else {
        handle_post(id, path, postdata);
    }
}

EMSCRIPTEN_KEEPALIVE
void stream_control(char* message)
{
    subscription::handleSubscriptioinMessage(nlohmann::json::parse(message), {});
}
}
void dispatch(BufferedRequest::Post& post)
{
    handle_post(post.id, post.path, post.postdata);
}

void dispatch(BufferedRequest::Get& get)
{
    handle_get(get.id, get.path);
}

void virtual_endpoint_initialize()
{
    std::lock_guard l(m);
    useBuffer = false;
    for (auto& e : requestBuffer) {
        std::visit([](auto& e) { dispatch(e); }, e.variant);
    }
    requestBuffer.clear();
};
