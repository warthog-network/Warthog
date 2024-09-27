callWithJson=function(name,json){
    if (typeof Module[name] == "function"){
        Module[name](UTF8ToString(json));
    }
}

addToLibrary({
    $VIRTUALSTATE: {
        dataChannelsMap: {},
        nextId: 1,
        getId: function() {
            var ret = VIRTUALSTATE.nextId++
            if (VIRTUALSTATE.nextId> 1000000000)
                VIRTUALSTATE.nextId=1;
            return ret;
        },
        getPromise: function(id){
            var p = new Promise((resolve, reject) => {
                if (VIRTUALSTATE[id]) {
                    VIRTUALSTATE[id].reject();
                }
                VIRTUALSTATE[id] = {resolve: resolve, reject: reject};
            });
            return p;
        },
        resolvePromise: function(id, json){
            if (VIRTUALSTATE[id]) {
                VIRTUALSTATE[id].resolve(json);
                delete VIRTUALSTATE[id];
            }
        }
    },
    $virtual_get(url){
        var id = VIRTUALSTATE.getId();
        Module._virtual_get_request(id, stringToNewUTF8(url));
        return VIRTUALSTATE.getPromise(id);
    },
    $virtual_get__deps: ['$VIRTUALSTATE'],

    $virtual_post(url,postdata){
        var id = VIRTUALSTATE.postId();
        Module._virtual_post_request(id, stringToNewUTF8(url), stringToNewUTF8(postdata));
        return VIRTUALSTATE.getPromise(id);
    },
    $virtual_post__deps: ['$VIRTUALSTATE'],

    onAPIResult: function(id, json){
        VIRTUALSTATE.resolvePromise(id,JSON.parse(UTF8ToString(json)));
    },
    onAPIResult__deps: ['$virtual_get', '$virtual_post'],
    onConnect: json => {
        if (typeof Module["onConnect"] == "function"){
            Module["onConnect"](JSON.parse(UTF8ToString(json)));
        }
    },
    onDisconnect: json => {
        if (typeof Module["onDisconnect"] == "function"){
            Module["onDisconnect"](JSON.parse(UTF8ToString(json)));
        }
    },
    onChain: json => {
        if (typeof Module["onChain"] == "function"){
            Module["onChain"](JSON.parse(UTF8ToString(json)));
        }
    },
    onMempoolAdd: json => {
        if (typeof Module["onMempoolAdd"] == "function"){
            Module["onMempoolAdd"](JSON.parse(UTF8ToString(json)));
        }
    },
    onMempoolErase: json => {
        if (typeof Module["onMempoolErase"] == "function"){
            Module["onMempoolErase"](JSON.parse(UTF8ToString(json)));
        }
    },
    onStream: json => {
        var f = Module["onStream"]
        if (typeof f == "function"){
            f(JSON.parse(UTF8ToString(json)));
        }
    },
    $stream_control(msg){
        Module._stream_control(stringToNewUTF8(JSON.stringify(msg)))
    },
    onStream__deps: ['$stream_control'],
});
