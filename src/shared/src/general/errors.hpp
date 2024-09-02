#pragma once

#include "errors_forward.hpp"
#include <cstdint>
#include <string>
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
#define ADDITIONAL_ERRNO_MAP(XX)                                                   \
    /*001 - 200: Errors*/                                                          \
    XX(1, EMSGTYPE, "invalid message type")                                        \
    XX(2, EMSGLEN, "invalid message length")                                       \
    XX(4, ECHECKSUM, "bad message checksum")                                       \
    XX(5, EMSGFLOOD, "received too many messages")                                 \
    XX(6, ENOBATCH, "peer did not provide batch")                                  \
    XX(7, EBUFFERFULL, "send buffer full")                                         \
    XX(8, EBATCHSIZE, "invalid batch size")                                        \
    XX(9, EHEADERLINK, "bad header link")                                          \
    XX(10, EPOW, "bad proof of work")                                              \
    XX(11, ETIMESTAMP, "timestamp rule violated")                                  \
    XX(12, EDIFFICULTY, "wrong difficulty in block header")                        \
    XX(13, EHANDSHAKE, "bad hand shake")                                           \
    XX(14, EVERSION, "unsupported version")                                        \
    XX(15, EREORGWORK, "peer changed to shorter chain")                            \
    XX(16, EDESCRIPTOR, "descriptors not consecutive")                             \
    XX(17, EMROOT, "merkle root mismatch")                                         \
    XX(18, ENOBLOCK, "peer did not provide block")                                 \
    XX(19, EUNREQUESTED, "received unrequested message")                           \
    XX(20, EIDPOLICY, "block transaction id policy violated")                      \
    XX(21, EADDRPOLICY, "new address policy violated")                             \
    XX(22, EBALANCE, "insufficient balance")                                       \
    XX(23, ECORRUPTEDSIG, "corrupted signature")                                   \
    XX(24, EINVACCOUNT, "invalid account id")                                      \
    XX(25, ETIMEOUT, "connection request timed out")                               \
    XX(26, ESWITCHING, "busy, switching chains")                                   \
    XX(27, ENONCE, "duplicate transaction nonce")                                  \
    XX(28, EDUST, "fee too low")                                                   \
    XX(29, EBLOCKSIZE, "block too large")                                          \
    XX(30, EPINHEIGHT, "invalid transaction pin")                                  \
    XX(31, ECLOCKTOLERANCE, "clock tolerance exceeded")                            \
    XX(32, EINVDSC, "invalid descripted state")                                    \
    XX(33, EAPPEND, "invalid chain append")                                        \
    XX(34, EFORK, "invalid chain fork")                                            \
    XX(57, ENOTFOUND, "not found")                                                 \
    XX(58, EEMPTY, "empty response for request not yet expired")                   \
    XX(59, EFAKEHEIGHT, "fake height advertised by node")                          \
    XX(60, EFAKEWORK, "fake total work advertised by node")                        \
    XX(61, EBADMATCH, "bad headerchain match")                                     \
    XX(62, EBADMISMATCH, "bad headerchain mismatch")                               \
    XX(63, EBADPROBE, "inconsistent probe message")                                \
    XX(64, EPROBEDESCRIPTOR, "current probe descriptor does not match")            \
    XX(65, ERESTRICTED, "peer ignored limit restrictions")                         \
    XX(66, ENOPINHEIGHT, "height is no pin height")                                \
    XX(67, EBADLEADER, "bad leader signature")                                     \
    XX(68, ELEADERMISMATCH, "leader signature mismatch")                           \
    XX(69, ELOWPRIORITY, "low leader signature priority")                          \
    XX(70, EBADPUBKEY, "invalid public key")                                       \
    XX(71, EBADPRIVKEY, "invalid private key")                                     \
    XX(72, EBADADDRESS, "invalid address")                                         \
    XX(73, EBADHEIGHT, "invalid height")                                           \
    XX(74, EZEROHEIGHT, "invalid zero height")                                     \
    XX(75, EBADROLLBACK, "rollback forbidden")                                     \
    XX(76, EBADROLLBACKLEN, "bad rollback length")                                 \
    XX(77, EMINEDDEPRECATED, "submitted deprecated block")                         \
    XX(78, EBLOCKRANGE, "invalid block range")                                     \
    XX(79, EFORKHEIGHT, "invalid fork height")                                     \
    XX(80, EPROBEHEIGHT, "invalid probe height")                                   \
    XX(81, EBATCHHEIGHT, "invalid batch height")                                   \
    XX(82, EGRIDMISMATCH, "grid mismatch")                                         \
    XX(83, ESELFSEND, "self send transaction not allowed")                         \
    XX(84, EBLOCKVERSION, "unsupported block version")                             \
    XX(85, EZEROAMOUNT, "transactions cannot send 0 WART")                         \
    XX(86, ENOINIT, "first message must be init message")                          \
    XX(87, EINVINIT, "only first message can be init message")                     \
    XX(88, EFAKEACCID, "fake account id")                                          \
    XX(89, EINV_FUNDS, "malformed funds data")                                     \
    XX(90, EINV_BODY, "malformed body data")                                       \
    XX(91, EINV_PAGE, "invalid page")                                              \
    XX(92, EINV_PROBE, "invalid probe message")                                    \
    XX(93, EINV_GRID, "invalid grid")                                              \
    XX(94, EINV_TXREQ, "invalid tx request")                                       \
    XX(95, EINV_ARGS, "invalid API arguments")                                     \
    XX(96, EINV_TXREP, "invalid tx reply")                                         \
    XX(97, EINV_INITGRID, "invalid grid in init message")                          \
    XX(98, EINV_HEADERVEC, "invalid header vector")                                \
    XX(99, EINV_BLOCKREPSIZE, "invalid block reply size")                          \
    XX(100, EINV_RTCOFFER, "Invalid SDP offer forward request")                    \
    XX(101, EDUP_RTCOFFER, "Duplicate SDP offer forward request")                  \
    XX(102, ERTCINV_RFA, "Invalid RTC request forward answer")                     \
    XX(103, ERTCDUP_RFA, "Duplicate RTC request forward answer")                   \
    XX(104, ERTCINV_FA, "Invalid RTC forwarded answer")                            \
    XX(105, ERTCDUP_FA, "Duplicate RTC forwarded answer")                          \
    XX(106, ERTCIP_FA, "WebRTC forwarded ip differs")                              \
    XX(107, ERTCDISCARD_FA, "Invalid discarding of expected forwarded answer")     \
    XX(108, ERTCQUOTA_FO, "RTC quota for forwarded offers exceeded")               \
    XX(110, ERTCDUP_ID, "Duplicate RTC id")                                        \
    XX(111, ERTCFAILED, "WebRTC connection failed")                                \
    XX(112, ERTCCHANNEL_ERROR, "WebRTC channel error")                             \
    XX(113, ERTCCLOSED, "WebRTC connection was closed")                            \
    XX(114, ERTCTEXT, "Text over RTC is not supported")                            \
    XX(115, ERTCUNIQUEIP, "WebRTC SDP ip is not unique")                           \
    XX(116, ERTCUNIQUEIP_RFA, "WebRTC SDP ip not unique (forward answer)")         \
    XX(117, ERTCWRONGIP_RFA, "Wrong WebRTC SDP ip (forward answer)")               \
    XX(118, ERTCWRONGIP_FO, "Wrong WebRTC SDP ip (forward offer)")                 \
    XX(119, ERTCUNIQUEIP_RFO, "WebRTC forward offer request SDP ip is not unique") \
    XX(120, ERTCUNVERIFIEDIP, "WebRTC ip not verified")                            \
    XX(121, ERTCDUP_DATACHANNEL, "WebRTC ip not verified")                         \
    XX(122, ERTCUNEXP_VA, "Unexpected WebRTC verification answer")                 \
    /*200 - 299: Errors not leading to ban*/                                       \
    XX(200, ERTCNOSIGNAL, "WebRTC signaling server was closed (offer)")            \
    XX(201, ERTCNOSIGNAL2, "WebRTC signaling server was closed (answer)")          \
    XX(202, EMSGINTEGRITY, "message integrity check failed")                       \
    XX(203, ESTARTWEBSOCK, "Error while starting websocket connection")            \
    XX(204, EWEBSOCK, "Error while in websocket connection")                       \
    XX(205, ERTCOWNIP, "Own announced IP not present in ")                         \
    XX(206, ERTCFWDREJECT, "WebRTC forward rejected")                              \
    XX(207, ERTCCHANNEL_CLOSED, "WebRTC datachannel closed")                       \
    XX(208, ERTCNOPEER, "WebRTC verification peer already closed")                 \
    XX(209, ERTCNOIP, "Cannot select own WebRTC ip")                               \
    XX(210, ERTCFEELER, "Normal feeler connection shutdown")                       \
    /*300 - 399: API triggered errors*/                                            \
    XX(300, EINV_HEX, "cannot parse hexadecimal input")                            \
    XX(301, EBADNONCE, "cannot parse nonce")                                       \
    XX(302, EBADFEE, "invalid fee")                                                \
    XX(303, EINEXACTFEE, "inexact fee not allowed")                                \
    XX(304, EBADAMOUNT, "invalid amount")                                          \
    XX(305, EPARSESIG, "cannot parse signature")                                   \
    XX(306, ENOTSYNCED, "node not synced yet")                                     \
    XX(1000, ESIGTERM, "received SIGTERM")                                         \
    XX(1001, ESIGHUP, "received SIGHUP")                                           \
    XX(1002, ESIGINT, "received SIGINT")                                           \
    XX(1003, EREFUSED, "connection refused due to ban")                            \
    XX(1004, EMAXCONNECTIONS, "too many connections from this ip")                 \
    XX(1005, EDUPLICATECONNECTION, "duplicate connection")                         \
    XX(1006, EEVICTED, "connection was evicted")                                   \
    XX(2000, EBUG, "bug-related error")

#define ERR_DEFINE(code, name, _) constexpr int32_t name = code;
ADDITIONAL_ERRNO_MAP(ERR_DEFINE)
#undef ERR_DEFINE

namespace errors {
const char* strerror(int32_t code);
const char* err_name(int32_t code);
inline bool leads_to_ban(int32_t code)
{
    if (code <= 0 || code >= 200)
        return false;
    switch (code) {
    case ECHECKSUM:
        //
        // We are not sure the following are triggered by evil behavior or bug.
        // Let's observe for some more time before enable banning on them.
    case EEMPTY:
    case EPROBEDESCRIPTOR:
        return false;
    default:
        return true;
    }
    return code != ECHECKSUM;
}
} // namespace errors

inline const char* Error::strerror() const { return errors::strerror(code); }
inline const char* Error::err_name() const { return errors::err_name(code); }
inline std::string Error::format() const { return std::string(err_name()) + " (" + strerror() + ")"; }
