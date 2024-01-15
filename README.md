WARTHOG REFERENCE IMPLEMENTATION
Copyright (c) 2023 Pumbaa, Timon & Rafiki
<p align="center">
  <img src="doc/img/warthog_logo.png" style="width:300px;"/>
</p>

# 🐗 Warthog Reference Implementation

Welcome to Warthog!

Warthog is an experimental L1-cryptocurrency implementation without
specific focus. Don't take this project seriously, it is just 
our hobby experiment. This project is not a dumb fork of something else. It was developed completely from scratch! 



## 💵 Tokenomics
We are a cryptocurrency for the community where everyone can revive again the good old days when crypto was fun. Therefore we have decided to be free of 💩bullshit:
- 👍 No Premine
- 🤟 No Team/Dev fund
- 😊 100% of supply is publicly mineable
<p align="center">
  <img src="doc/img/tokenomics.png" />
</p>

The block chain has the following characteristics:
- 1️⃣  Coin unit: 1 WART
- ⏲  Block time: 20s
- 💰 Initial block reward: 3 WART
- 🧮 Precision: 0.00000001 WART (8 digits)
- 🔪 Halving: ~ every 2 years
- 🔒 Supply hard cap : 18921599.68464 (~19m) WART


## 💣 A word of caution
But beware of bugs: Here be dragons. Almost everything was 
implemented from scratch. Don't expect this
software to be rock solid, and don't even expect things to work 
properly. Use at your own risk. There may be and almost 
certainly are serious bugs! You have been warned.

Have fun with it. Like we had implementing it.

## 📢 Socials

<p align="center">
<a href="https://discord.gg/QMDV8bGTdQ"><img src="doc/img/discord.png" alt="drawing" style="width:16px;"/> Discord </a>
| 
<a href="https://t.me/warthognetwork"><img src="doc/img/telegram.png" alt="drawing" style="width:16px;"/> Telegram </a>
| 
 <a href="https://bitcointalk.org/index.php?topic=5458046.0"> <img src="doc/img/bitcointalk.png" alt="drawing" style="width:16px;"/> Bitcointalk</a>
 |
 <a href="http://warthog.network">🌐 Website</a>
</p>

## 💱 Where to buy?
Not listed yet. At the moment only p2p on Discord. 


## 📦 Component overview

### This Repo
* Reference node implementation of the Warthog Network
* Command line wallet software

### Miner
* GPU/CPU Miner for JanusHash [here](https://github.com/CoinFuMasterShifu/janusminer)

### Additional Tools:
* GUI wallet [here](https://github.com/warthog-network/wart-wallet)
 
## 💻 Installation
Prebuilt binaries of the node daemon and cli wallet for Linux and Windows can be downloaded [here](https://github.com/warthog-network/Warthog/releases). They are staticlly linked and will just work without external dependencies.

Prebuilt binaries of the miner for Linux and HiveOS can be downloaded [here](https://github.com/CoinFuMasterShifu/janusminer/releases)

To compile from source see below or [here](https://github.com/warthog-network/warthog-guide) for a more detailed guide.

## 😵‍💫 BUILD INSTRUCTIONS

### Linux Native Build

#### System Requirements

* Linux
* gcc11 or newer
* meson
* ninja

#### Required Steps
* Install gcc, meson, ninja: apt install meson ninja-build build-essential
* Clone the repo: `git clone https://github.com/ByPumbaa/Warthog`
* cd into the repo: `cd Warthog`
* Create build directory: `meson build .` (`meson build . --buildtype=release` for better performance)
* cd into build directory: `cd build`
* Compile using ninja: `ninja`

### Docker build (node and wallet)
#### System Requirements
* Linux
* Docker

#### Build for Linux
* Run `DOCKER_BUILDKIT=1 docker build . --output build` in the repo directory.
#### Build for Windows (cross-compilation on Linux)
* Run `DOCKER_BUILDKIT=1 docker build . -f DockerfileWindows --output ./build/windows` in the repo.
* Windows binaries are located in `./build/windows` directory.
#### Build for MacOS - aarch64 (cross-compilation on Linux)
* Run `DOCKER_BUILDKIT=1 docker build . -f DockerfileMacOS --output ./build/macos` in the repo.
* MacOS binaries are located in `./build/macos` directory.

## ▶️ USAGE
* Run the node (use some restarter in case it crashes) <br />
One line example to run the node: `screen -dmS wart_node bash -c "while true; do wart-node-linux ; done"` <br />
Use `screen -r wart_node` to see its output and CTRL+A+D to detach from the screen session. <br />
Note: You should run node with  `--rpc=0.0.0.0:3000` to accept remote connections from your other rigs. <br />
* Run the miner (miner requires node running). 
More detailed information how to set up and run the miner you can find [here](https://github.com/CoinFuMasterShifu/janusminer/blob/master/README.md).
* Optional: Run the wallet to send funds (wallet requires node running)
* Good luck and have fun! Use --help the option.

NOTE: We (or some of us) might drop this project any time in case 
      deep/unfixable bugs or ugly design issues arise. Or without
      any reason and without prior notice. This is a highly 
      experimental project.

## 📖 Documentation
* [API Reference](./doc/API.md)
* [Python Integration Guide](./doc/integration_python.md)
* [Elixir Integration Guide](./doc/integration_elixir.md)
* [Nodejs Integration Guide](./doc/integration_nodejs.md)

## ⛏ Upcoming Mining algorithm
 * NEWS: Custom [Janushash algorithm](./doc/janushash.md) in the works. First Proof of Balanced Work mining algorithm in the world.

