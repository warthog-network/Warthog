#pragma once
#include "eventloop/types/conref_declaration.hpp"
#include <concepts>
class Eventloop;
struct IsRequest;
class Conref;
class RequestSender {
public:
    template <typename T>
    requires std::derived_from<T, IsRequest>
    void send(Conref cr, const T& req);
    bool finished();

private:
    RequestSender(const RequestSender&) = default;
    RequestSender(RequestSender&&) = delete;
    friend class Eventloop;
    RequestSender(Eventloop& e)
        : e(e)
    {
    }
    Eventloop& e;
};
