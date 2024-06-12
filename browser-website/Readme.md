# How to use the browser-website

## For testing/develpment

* Copy the built files to the src directory: `wart-node.js`, `wart-node.wasm`, `wart-node.worker.js`
* Start a webserver `python3 server.py` (root directory of repo) which serves the relevant headers


## For production
* Copy the built files to the src directory: `wart-node.js`, `wart-node.wasm`, `wart-node.worker.js`
* adjust the websocket warthog servers (TODO: make this configurable)
* Host the files from the src directory and make sure the relevant headers from `server.py` are present.
