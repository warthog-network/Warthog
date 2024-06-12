callWithJson=function(name,json){
    if (typeof Module[name] == "function"){
        Module[name](UTF8ToString(json));
    }
}

mergeInto(LibraryManager.library, {
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
});
