#include "api_call.hpp"
#include "cmdline/cmdline.h"
#include "communication/create_payment.hpp"
#include "crypto/crypto.hpp"
#include "general/hex.hpp"
#include "general/params.hpp"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace nlohmann;

struct Wallet {
    json parsed;
    PrivKey privKey;
    PubKey pubKey;
    Address address;
    Wallet(const std::string& s)
        : parsed(json::parse(s))
        , privKey(parsed["privateKey"].get<std::string>())
        , pubKey(privKey.pubkey())
        , address(pubKey.address())
    {
        std::string pubKeyString = parsed["publicKey"].get<std::string>();
        std::string addressString = parsed["address"].get<std::string>();
        if ((PubKey(pubKeyString) != pubKey) || (Address(addressString) != address)) {
            throw std::runtime_error("Inconsistent data.");
        }
    }
};

int gen_wallet(const filesystem::path& path, bool printOnly, PrivKey k = PrivKey())
{
    if (!printOnly && std::filesystem::exists(path)) {
        cerr << "Cannot create wallet, file '" + path.string() + "' already exists. You can specify a filename using the '-f' "
                                                                 "option.\n";
        return -1;
    }
    {
        json j;
        j["privateKey"] = k.to_string();
        auto p { k.pubkey() };
        j["publicKey"] = p.to_string();
        j["address"] = p.address().to_string();

        if (printOnly) {
            cout << j.dump(2);
        } else {
            ofstream os(path);
            if (os.good()) {
                os << j.dump();
                if (os.good())
                    cout << "Wallet created." << endl;
                else
                    cerr << "Could not create wallet, file is now corrupted :)\n"
                         << endl;
            } else {
                cerr << "Cannot create wallet" << endl;
                return -1;
            }
        }
    }
    return 0;
}

Wallet open_wallet(const filesystem::path& path)
{
    ifstream file(path);
    ostringstream ss;
    if (file.is_open()) {
        ss << file.rdbuf();
        try {
            Wallet w(ss.str());
            return w;
        } catch (std::runtime_error& e) {
            throw std::runtime_error(
                "Wallet file corrupted: "s + e.what() + ". You might want to restore using a private key."s);
        } catch (nlohmann::detail::parse_error& e) {
            throw std::runtime_error(
                "Wallet file corrupted: File has incorrect JSON structure. You might "
                "want to restore using a private key.");
        }
    } else {
        if (std::filesystem::exists(path)) {
            throw std::runtime_error("Cannot read file '" + path.string() + "'.");
        } else {
            throw std::runtime_error("Wallet file '" + path.string() + "' does not exist yet. Create a new wallet file "
                                                                       "using the '-c' flag.");
        }
    }
}
int print_address(const filesystem::path& path)
{
    ifstream file(path);
    ostringstream ss;
    if (file.is_open()) {
        ss << file.rdbuf();
        try {
            Wallet w(ss.str());
            cout << w.address.to_string() << endl;
        } catch (std::runtime_error& e) {
            cerr << "Wallet file corrupted: " << e.what()
                 << " You might want to restore using a private key." << endl;
        } catch (nlohmann::detail::parse_error& e) {
            cerr << "Wallet file corrupted: File has incorrect JSON structure. You "
                    "might want to restore using a private key."
                 << endl;
        }
    } else {
        if (std::filesystem::exists(path)) {
            cerr << "Cannot read file '" + path.string() + "'.";
        } else {
            cerr << "Wallet file '" + path.string() + "' does not exist yet. Create a new wallet file using the "
                                                      "'-c' flag.";
        }
        return -1;
    }
    return 0;
}

int process(gengetopt_args_info& ai)
{
    bool action = false;
    bool po_action = ai.print_only_given != 0;
    if (ai.print_only_given) {
        if ((!ai.restore_given && !ai.create_given) || ai.address_given || ai.balance_given || ai.send_given) {
            cerr << "option --print-only is only compatible with --create or --restore";
            return -1;
        }
    }
    Endpoint endpoint(ai.host_arg, ai.port_arg);
    try {
        filesystem::path walletpath(ai.file_arg);
        if (ai.create_given) {
            if (ai.restore_given) {
                cerr << "Invalid option combination (--create and --restore)." << endl;
                return -1;
            }
            if (int i = gen_wallet(walletpath, po_action))
                return i;
            action = true;
            po_action = false;
        }
        if (ai.restore_given) {
            try {
                PrivKey pk(ai.restore_arg);
                if (int i = gen_wallet(walletpath, po_action, pk))
                    return i;
                action = true;
                po_action = false;
            } catch (std::runtime_error& e) {
                cerr << "Cannot restore wallet: " << e.what() << endl;
                return -1;
            }
        }
        if (ai.print_only_given)
            return 0;

        Wallet w { open_wallet(walletpath) };

        if (ai.address_given) {
            cout << w.address.to_string() << endl;
            return 0;
        }
        if (ai.balance_given) {
            cout << endpoint.get_balance(w.address.to_string()).to_string() << endl;
            return 0;
        }
        if (ai.send_given) {
            if (!ai.amount_given || !ai.to_given || !ai.fee_given) {
                cerr << "Please specify the options '--to','--amount' and '--fee'."
                     << endl;
                return -1;
            }
            Address to(ai.to_arg);
            cout << "Get pin" << endl;
            auto pin = endpoint.get_pin();
            cout << "Got pin" << endl;
            auto fee { Funds::parse(ai.fee_arg) };
            auto amount { Funds::parse(ai.amount_arg) };
            if (!fee || !amount) {
                cerr << "Bad fee/amount" << endl;
                return -1;
            }
            NonceId nid { ai.nonce_given ? NonceId(ai.nonce_arg) : NonceId::random() };
            PaymentCreateMessage m(pin.first, pin.second, w.privKey, CompactUInt::compact(*fee), to, *amount, nid);
            assert(m.valid_signature(pin.second, w.address));
            cout << "NonceId: " << m.nonceId.value() << endl;
            cout << "pinHeight: " << m.pinHeight.value() << endl;
            cout << "pinHeight: " << pin.first.value() << endl;
            cout << "pinHash: " << serialize_hex(pin.second) << endl;
            std::string msg;
            int code = endpoint.send_transaction(m, &msg);
            if (code) {
                cout << "Transaction rejected (code " << code << "): " << msg;
                return -1;
            } else {
                cout << "Transaction accepted.";
                return 0;
            }
        }
    } catch (std::runtime_error& e) {
        cerr << e.what() << endl;
        ;
        return -1;
    }
    if (!action) {
        cerr << "Please specify some option. Print help using the '-h' flag."
             << endl;
        return -1;
    }
    return 0;
}
int main(int argc, char** argv)
{
    ECC_Start();
    gengetopt_args_info ai;
    if (cmdline_parser(argc, argv, &ai) != 0) {
        return -1;
    }
    int i = process(ai);
    cmdline_parser_free(&ai);
    ECC_Stop();
    return i;
}
