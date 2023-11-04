#pragma once

#include <cstdint>
////////////////////////////////////
// LIST OF ERROR CODES            //
////////////////////////////////////
// These codes are used to describe why a connection was
// closed by a peer or by our end.

// LIBUV ERROR CODES
// -----------------
// Libuv error codes  are negative and are returned
// by libuv routines are mainly network related such as
// host unreachable, address already in use, broken pipe
// etc.

// ADDITIONAL ERROR CODES
// -------------------
// Misbehaviour, range [1-999]:
// Other codes, range [1000-1999]
#define ADDITIONAL_ERRNO_MAP(XX)                                        \
    XX(1, EMSGTYPE, "invalid message type")                             \
    XX(2, EMSGLEN, "invalid message length")                            \
    XX(3, EMALFORMED, "malformed data")                                 \
    XX(4, ECHECKSUM, "bad message checksum")                            \
    XX(5, EMSGFLOOD, "received too many messages")                      \
    XX(6, ENOBATCH, "peer did not provide batch")                       \
    XX(7, EBUFFERFULL, "send buffer full")                              \
    XX(8, EBATCHSIZE, "invalid batch size")                             \
    XX(9, EHEADERLINK, "bad header link")                               \
    XX(10, EPOW, "bad proof of work")                                   \
    XX(11, ETIMESTAMP, "timestamp rule violated")                       \
    XX(12, EDIFFICULTY, "wrong difficulty in block header")             \
    XX(13, EHANDSHAKE, "bad hand shake")                                \
    XX(14, EVERSION, "unsupported version")                             \
    XX(15, EREORGWORK, "peer changed to shorter chain")                 \
    XX(16, EDESCRIPTOR, "descriptors not consecutive")                  \
    XX(17, EMROOT, "merkle root mismatch")                              \
    XX(18, ENOBLOCK, "peer did not provide block")                      \
    XX(19, EUNREQUESTED, "received unrequested message")                \
    XX(20, EIDPOLICY, "block transaction id policy violated")           \
    XX(21, EADDRPOLICY, "new address policy violated")                  \
    XX(22, EBALANCE, "insufficient balance")                            \
    XX(23, ECORRUPTEDSIG, "corrupted signature")                        \
    XX(24, EINVACCOUNT, "invalid account id")                           \
    XX(25, ETIMEOUT, "connection request timed out")                    \
    XX(26, ESWITCHING, "busy, switching chains")                        \
    XX(27, ENONCE, "duplicate transaction nonce")                       \
    XX(28, EDUST, "fee too low")                                        \
    XX(29, EBLOCKSIZE, "block too large")                               \
    XX(30, EPINHEIGHT, "invalid transaction pin")                       \
    XX(31, ECLOCKTOLERANCE, "clock tolerance exceeded")                 \
    XX(32, EINVDSC, "invalid descripted state")                         \
    XX(33, EAPPEND, "invalid chain append")                             \
    XX(34, EFORK, "invalid chain fork")                                 \
    XX(57, ENOTFOUND, "not found")                                      \
    XX(58, EEMPTY, "empty response for request not yet expired")        \
    XX(59, EFAKEHEIGHT, "fake height advertised by node")               \
    XX(60, EFAKEWORK, "fake total work advertised by node")             \
    XX(61, EBADMATCH, "bad headerchain match")                          \
    XX(62, EBADMISMATCH, "bad headerchain mismatch")                    \
    XX(63, EBADPROBE, "inconsistent probe message")                     \
    XX(64, EPROBEDESCRIPTOR, "current probe descriptor does not match") \
    XX(65, ERESTRICTED, "peer ignored limit restrictions")              \
    XX(66, ENOPINHEIGHT, "height is no pin height")                     \
    XX(67, EBADLEADER, "bad leader signature")                          \
    XX(68, ELEADERMISMATCH, "leader signature mismatch")                \
    XX(69, ELOWPRIORITY, "low leader signature priority")               \
    XX(70, EBADPUBKEY, "invalid public key")                            \
    XX(71, EBADPRIVKEY, "invalid private key")                          \
    XX(72, EBADADDRESS, "invalid address")                              \
    XX(73, EBADHEIGHT, "invalid height")                                \
    XX(74, EZEROHEIGHT, "invalid zero height")                          \
    XX(75, EBADROLLBACK, "rollback forbidden")                          \
    XX(76, EBADROLLBACKLEN, "bad rollback length")                      \
    XX(77, EMINEDDEPRECATED, "submitted deprecated block")              \
    XX(78, EBLOCKRANGE, "invalid block range")                          \
    XX(79, EFORKHEIGHT, "invalid fork height")                          \
    XX(80, EPROBEHEIGHT, "invalid probe height")                        \
    XX(81, EBATCHHEIGHT, "invalid batch height")                        \
    XX(82, EGRIDMISMATCH, "grid mismatch")                              \
    XX(83, ESELFSEND, "self send transaction not allowed")              \
    XX(84, EBLOCKVERSION, "unsupported block version")                  \
    XX(201, EBADNONCE, "cannot parse nonce")                            \
    XX(202, EBADFEE, "invalid fee")                                     \
    XX(203, EINEXACTFEE, "inexact fee not allowed")                     \
    XX(204, EBADAMOUNT, "invalid amount")                               \
    XX(205, EPARSESIG, "cannot parse signature")                        \
    XX(1000, ESIGTERM, "received SIGTERM")                              \
    XX(1001, ESIGHUP, "received SIGHUP")                                \
    XX(1002, ESIGINT, "received SIGINT")                                \
    XX(1003, EREFUSED, "connection refused due to ban")                 \
    XX(1004, EMAXCONNECTIONS, "too many connections from this ip")      \
    XX(1005, EDUPLICATECONNECTION, "duplicate connection")              \
    XX(2000, EBUG, "bug-related error")

#define ERR_DEFINE(code, name, _) constexpr int32_t name = code;
ADDITIONAL_ERRNO_MAP(ERR_DEFINE)
#undef ERR_DEFINE

namespace errors {
const char* strerror(int32_t code);
const char* err_name(int32_t code);
inline bool is_malicious(int32_t code)
{
    return code != ECHECKSUM && code > 0 && code < 100 && code != ECHECKSUM;
}
} // namespace errors

struct Error { // error class for exceptions
    Error(int32_t e = 0)
        : e(e) {};
    const char* strerror() const { return errors::strerror(e); };
    const char* err_name() const { return errors::err_name(e); };
    bool is_error() const { return e != 0; }
    operator bool() const { return is_error(); }
    int32_t e;
};

class NonzeroHeight;
struct ChainError : public Error {
    // ChainError() {};
    ChainError(int32_t e, NonzeroHeight height);
    NonzeroHeight height() const;

private:
    uint32_t h;
};
