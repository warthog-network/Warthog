# Websocket Endpoint on network_refactor branch

Nodes can communicate over websocket to accept connections from browser nodes and negotiate direct peer-to-peer WebRTC connections between browserr nodes and between native nodes and browser nodes.

## Enable Websocket server

There are three command line flags controlling the websocket server. By default it is disabled. They can be listed with the `--help` flag:

```
Websocket server options:
      --ws-port=INT          Websocket port
      --ws-tls-cert=STRING   TLS certificate file for public websocket endpoint
      --ws-tls-key=STRING    TLS private key file for public websocket endpoint
```

Only when `--ws-port=<port>` is specified, the websocket server is enabled. TLS encryption is enabled when *both* files, specified with `--ws-tls-cert` and `--ws-tls-key` are found. The default file names are `ws.cert` and `ws.key`.

### Example using self-signed certificate:
As in [this example](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/http-server/minimal-http-server-tls?id=2c2969cdac8d5b156dedef40b335f5733dd0f821), self-signed certificates can be created as follows:
```bash
echo -e "GB\nErewhon\nAll around\nlibwebsockets-test\n\nlocalhost\nnone@invalid.org\n" | openssl req -new -newkey rsa:4096 -days 36500 -nodes -x509 -keyout "ws.key" -out "ws.cert"
```




