#pragma once
#include <cassert>
#include <functional>
#include <ostream>
#include <vector>
namespace router {
namespace {

    struct RouteParser;
    struct ParsedRoute {
        RouteParser& parser;
        std::vector<std::string_view> args;
    };
    inline std::string_view normalize_slash(std::string_view s)
    {
        while (s.ends_with('/'))
            s = s.substr(0, s.size() - 1);
        return s;
    }
    using id_t = int;

    enum class RequestType { GET,
        POST };
    using Args = std::vector<std::string_view>;
    struct Request {
        std::string_view url;
        Args& args;
        std::string_view getParameter(size_t i)
        {
            if (args.size() > i)
                return args[i];
            return {};
        }
        std::string_view getUrl() const { return url; }
    };

    struct Response {
        id_t id;
        std::string_view postData;
        void onData(auto cb)
        {
            cb(postData, true);
        }
    };

    struct ChunkNode;
    struct NextNodes {
        [[nodiscard]] ChunkNode* match_first_character(std::string_view s);
        [[nodiscard]] const ChunkNode*
        match_first_character(std::string_view s) const;
        ChunkNode& insert(std::string_view cn);
        ChunkNode& push_back(std::string_view cn);
        std::vector<ChunkNode> list;
    };

    struct NodeData {
        using cb_t = std::function<void(Response*, Request*)>;
        cb_t callback;
        RequestType requestType;
    };

    struct ArgNode {
        NextNodes nextNodes;
        std::optional<NodeData> data;
    };

    struct NextAndArgNodes {
        NextNodes nextNodes;
        std::unique_ptr<ArgNode> argNode;
    };

    struct ChunkNode : public NextAndArgNodes, public NodeData {
        ChunkNode(std::string s)
            : chunk(std::move(s))
        {
        }
        char first_char() const { return chunk[0]; }
        ChunkNode& insert(std::string_view s)
        {
            const size_t N { std::min(s.size(), chunk.size()) };
            size_t i { 0 };
            for (; i < N; ++i) {
                if (s[i] != chunk[i])
                    break;
            }
            if (i < chunk.size()) {
                auto nn { std::move(nextNodes) };
                auto& cn { nextNodes.push_back({ chunk.substr(i, chunk.size() - i) }) };
                chunk.resize(i);
                cn.nextNodes = std::move(nn);
                cn.argNode = std::move(argNode);
                cn.data = std::move(data);
                data.reset();
            }
            if (i < s.size()) {
                return nextNodes.insert(s.substr(i, s.size() - i));
            } else {
                return *this;
            }
        }
        std::string chunk;
        std::optional<NodeData> data;
    };

    inline ChunkNode& NextNodes::push_back(std::string_view cn)
    {
        list.push_back({ std::string(cn) });
        return list.back();
    }

    inline ChunkNode* NextNodes::match_first_character(std::string_view s)
    {
        assert(s.size() > 0);
        for (auto& e : list) {
            if (e.chunk[0] == s[0]) {
                return &e;
            };
        }
        return nullptr;
    }

    inline const ChunkNode*
    NextNodes::match_first_character(std::string_view s) const
    {
        assert(s.size() > 0);
        for (auto& e : list) {
            if (e.chunk[0] == s[0]) {
                return &e;
            };
        }
        return nullptr;
    }

    inline ChunkNode& NextNodes::insert(std::string_view s)
    {
        if (auto p { match_first_character(s) })
            return p->insert(s);
        return push_back(s);
    }

    struct ParsedNodes {
        struct ParsedArgNode;
        struct ParsedChunkNode {
            ParsedChunkNode(ChunkNode n)
                : node(std::move(n))
            {
            }
            ChunkNode node;
            std::unique_ptr<ParsedArgNode> next;
        };
        struct ParsedArgNode {
            ArgNode node;
            std::unique_ptr<ParsedChunkNode> next;
        };
        using variant_t = std::variant<std::unique_ptr<ParsedArgNode>,
            std::unique_ptr<ParsedChunkNode>>;
        void visit(auto chunkHandler, auto argHandler) const
        {
            ParsedArgNode* parg { nullptr };
            ParsedChunkNode* pchunk { nullptr };
            if (std::holds_alternative<std::unique_ptr<ParsedArgNode>>(variant)) {
                parg = std::get<std::unique_ptr<ParsedArgNode>>(variant).get();
                goto arg;
            }
            pchunk = std::get<std::unique_ptr<ParsedChunkNode>>(variant).get();
            while (true) {
                if (!pchunk)
                    break;
                chunkHandler(pchunk->node);
                parg = pchunk->next.get();
            arg:
                if (!parg)
                    break;
                argHandler(parg->node);
                pchunk = parg->next.get();
            }
        }
        friend std::ostream& operator<<(std::ostream& os, const ParsedNodes& n)
        {
            size_t i = 0;
            n.visit(
                [&](const ChunkNode& node) { os << node.chunk; },
                [&](const ArgNode&) { os << ":arg" << i++ << ":"; });
            return os;
        }
        ParsedNodes(std::string_view s)
            : variant(parse(s))
        {
        }

    private:
        variant_t variant;
        static variant_t parse(std::string_view route)
        {
            route = normalize_slash(route);
            assert(route.size() != 0);
            size_t start = 0;

            auto arg_tok = [&]() {
                start = route.find("/", start);
                return std::make_unique<ParsedArgNode>();
            };
            auto chunk_tok { [&]() {
                auto end = route.find(':', start);
                auto s { end == std::string::npos ? route.substr(start)
                                                  : route.substr(start, end - start) };
                start = end;
                return std::make_unique<ParsedChunkNode>(ChunkNode { std::string(s) });
            } };

            ParsedArgNode* pan { nullptr };
            variant_t var { [&]() -> variant_t {
                if (route[0] == ':') {
                    auto t { arg_tok() };
                    pan = t.get();
                    return variant_t { std::move(t) };
                } else {
                    auto t { chunk_tok() };
                    auto& ref { *t };
                    if (start == std::string::npos)
                        return t;
                    ref.next = arg_tok();
                    pan = ref.next.get();
                    return t;
                }
            }() };
            while (true) {
                if (start == std::string::npos)
                    break;
                pan->next = chunk_tok();
                if (start == std::string::npos)
                    break;
                pan->next->next = arg_tok();
                pan = pan->next->next.get();
            }
            return var;
        }
    };

    struct Router {
        bool get(std::string_view s, NodeData::cb_t cb)
        {
            return insert(ParsedNodes(s), std::move(cb), RequestType::GET);
        }
        bool post(std::string_view s, NodeData::cb_t cb)
        {
            return insert(ParsedNodes(s), std::move(cb), RequestType::POST);
        }

        bool match_get(id_t id, std::string_view url)
        {
            if (auto r { match(url, RequestType::GET) }; r) {
                Response res { id, {} };
                Request req { url, r->args };
                r->cb(&res, &req);
                return true;
            }
            return false;
        }
        bool match_post(id_t id, std::string_view url, std::string_view postData)
        {
            if (auto r { match(url, RequestType::POST) }; r) {
                Response res { id, postData };
                Request req { url, r->args };
                r->cb(&res, &req);
                return true;
            }
            return false;
        }

    private:
        struct MatchResult {
            const NodeData::cb_t& cb;
            Args args;
        };

        [[nodiscard]] std::optional<MatchResult> match(std::string_view s, RequestType rt)
        {
            auto [p, args] { match_node(s) };
            if (!p || !*p)
                return {};
            auto& d { **p };
            if (d.requestType == rt) {
                return MatchResult { d.callback, std::move(args) };
            }
            return {};
        }
        std::pair<std::optional<NodeData>*, Args> match_node(std::string_view s)
        {
            s = normalize_slash(s);
            NextNodes* nn { &rootNodes.nextNodes };
            auto* an { rootNodes.argNode.get() };
            std::optional<NodeData>* res { nullptr };
            Args args;
            while (true) {
                if (s.size() == 0)
                    return { res, std::move(args) };
                if (auto p { nn->match_first_character(s) }) {
                    if (!s.starts_with(p->chunk)) {
                        return { nullptr, std::move(args) };
                    }
                    s = s.substr(p->chunk.size());
                    nn = &p->nextNodes;
                    an = p->argNode.get();
                    res = &p->data;
                } else {
                    if (!an)
                        return { nullptr, std::move(args) };
                    res = &an->data;
                    nn = &an->nextNodes;
                    an = nullptr;
                    auto pos { s.find('/') };
                    args.push_back(s.substr(0, pos));
                    if (pos != std::string::npos) {
                        s = s.substr(pos);
                    } else {
                        s = {};
                    }
                }
            }
        }
        bool insert(const ParsedNodes& pn, NodeData::cb_t cb, RequestType rt)
        {
            auto* pNodes { &rootNodes };
            std::optional<NodeData>* pNodeData { nullptr };
            NextNodes* nn { &rootNodes.nextNodes };
            pn.visit(
                [&](const ChunkNode& n) {
                    auto p = &nn->insert(n.chunk);
                    pNodes = p;
                    pNodeData = &p->data;
                    nn = &p->nextNodes;
                },
                [&](const auto&) {
                    auto& an { pNodes->argNode };
                    if (!an)
                        an = std::make_unique<ArgNode>();
                    pNodeData = &an->data;
                    nn = &an->nextNodes;
                });
            assert(pNodeData);
            auto& data { *pNodeData };
            if (data)
                return false;
            data = NodeData { std::move(cb), rt };
            return true;
        }

        NextAndArgNodes rootNodes;
    };
}
}
