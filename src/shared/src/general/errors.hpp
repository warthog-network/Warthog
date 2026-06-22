#pragma once

#include "general/errors_forward.hpp"
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
#define ADDITIONAL_ERRNO_MAP(XX)                                                   \
    /*001 - 200: Errors*/                                                          \
    XX(0, ENOERROR, "No error")                                                    \
    XX(1, EMSGTYPE, "Invalid message type")                                        \
    XX(2, EMSGLEN, "Invalid message length")                                       \
    XX(4, ECHECKSUM, "Bad message checksum")                                       \
    XX(5, EMSGFLOOD, "Received too many messages")                                 \
    XX(6, ENOBATCH, "Peer did not provide batch")                                  \
    XX(7, EBUFFERFULL, "Send buffer full")                                         \
    XX(8, EBATCHSIZE, "Invalid batch size")                                        \
    XX(9, EHEADERLINK, "Bad header link")                                          \
    XX(10, EPOW, "Bad proof of work")                                              \
    XX(11, ETIMESTAMP, "Timestamp rule violated")                                  \
    XX(12, EDIFFICULTY, "Wrong difficulty in block header")                        \
    XX(13, EHANDSHAKE, "Bad hand shake")                                           \
    XX(14, EVERSION, "Unsupported version")                                        \
    XX(15, EREORGWORK, "Peer changed to shorter chain")                            \
    XX(16, EDESCRIPTOR, "Descriptors not consecutive")                             \
    XX(17, EMROOT, "Merkle root mismatch")                                         \
    XX(18, ENOBLOCK, "Peer did not provide block")                                 \
    XX(19, EUNREQUESTED, "Received unrequested message")                           \
    XX(20, EIDNOTREFERENCED, "Account id not referenced")                          \
    XX(21, EADDRPOLICY, "New address policy violated")                             \
    XX(22, EBALANCE, "Insufficient balance")                                       \
    XX(23, ECORRUPTEDSIG, "Corrupted signature")                                   \
    XX(24, ETIMEOUT, "Connection request timed out")                               \
    XX(25, ESWITCHING, "Busy, switching chains")                                   \
    XX(26, ENONCE, "Duplicate transaction nonce")                                  \
    XX(27, EDUST, "Fee too low")                                                   \
    XX(28, EBLOCKSIZE, "Block too large")                                          \
    XX(29, EPINHEIGHT, "Invalid transaction pin")                                  \
    XX(30, ECLOCKTOLERANCE, "Clock tolerance exceeded")                            \
    XX(31, EINVDSC, "Invalid descripted state")                                    \
    XX(32, EAPPEND, "Invalid chain append")                                        \
    XX(33, EFORK, "Invalid chain fork")                                            \
    XX(56, ENOTFOUND, "Not found")                                                 \
    XX(57, EEMPTY, "Empty response for request not yet expired")                   \
    XX(58, EFAKEHEIGHT, "Fake height advertised by node")                          \
    XX(59, EFAKEWORK, "Fake total work advertised by node")                        \
    XX(60, EBADMATCH, "Bad headerchain match")                                     \
    XX(61, EBADMISMATCH, "Bad headerchain mismatch")                               \
    XX(62, EBADPROBE, "Inconsistent probe message")                                \
    XX(63, EPROBEDESCRIPTOR, "Current probe descriptor does not match")            \
    XX(64, ERESTRICTED, "Peer ignored limit restrictions")                         \
    XX(65, ENOPINHEIGHT, "Height is no pin height")                                \
    XX(66, EBADLEADER, "Bad leader signature")                                     \
    XX(67, ELEADERMISMATCH, "Leader signature mismatch")                           \
    XX(68, ELOWPRIORITY, "Low leader signature priority")                          \
    XX(69, EBADPUBKEY, "Invalid public key")                                       \
    XX(70, EBADPRIVKEY, "Invalid private key")                                     \
    XX(71, EBADADDRESS, "Invalid address")                                         \
    XX(72, EBADHEIGHT, "Invalid height")                                           \
    XX(73, EZEROHEIGHT, "Invalid zero height")                                     \
    XX(74, EBADROLLBACK, "Rollback forbidden")                                     \
    XX(75, EBADROLLBACKLEN, "Bad rollback length")                                 \
    XX(76, EMINEDDEPRECATED, "Submitted deprecated block")                         \
    XX(77, EBLOCKRANGE, "Invalid block range")                                     \
    XX(78, EFORKHEIGHT, "Invalid fork height")                                     \
    XX(79, EPROBEHEIGHT, "Invalid probe height")                                   \
    XX(80, EBATCHHEIGHT, "Invalid batch height")                                   \
    XX(81, EGRIDMISMATCH, "Grid mismatch")                                         \
    XX(82, ESELFSEND, "Self send transaction not allowed")                         \
    XX(83, EBLOCKVERSION, "Unsupported block version")                             \
    XX(84, EZEROAMOUNT, "Transactions cannot send 0 tokens")                       \
    XX(85, ENOINIT, "First message must be init message")                          \
    XX(86, EINVINIT, "Only first message can be init message")                     \
    XX(87, EFAKEACCID, "Fake account id")                                          \
    XX(88, EINV_FUNDS, "Malformed funds data")                                     \
    XX(89, EINV_BODY, "Malformed body data")                                       \
    XX(90, EINV_PAGE, "Invalid page")                                              \
    XX(91, EINV_PROBE, "Invalid probe message")                                    \
    XX(92, EINV_GRID, "Invalid grid")                                              \
    XX(93, EINV_TXREQ, "Invalid tx request")                                       \
    XX(94, EINV_ARGS, "Invalid API arguments")                                     \
    XX(95, EINV_TXREP, "Invalid tx reply")                                         \
    XX(96, EINV_INITGRID, "Invalid grid in init message")                          \
    XX(97, EINV_HEADERVEC, "Invalid header vector")                                \
    XX(98, EINV_BLOCKREPSIZE, "Invalid block reply size")                          \
    XX(99, EINV_RTCOFFER, "Invalid SDP offer forward request")                     \
    XX(100, EDUP_RTCOFFER, "Duplicate SDP offer forward request")                  \
    XX(101, ERTCINV_RFA, "Invalid RTC request forward answer")                     \
    XX(102, ERTCDUP_RFA, "Duplicate RTC request forward answer")                   \
    XX(103, ERTCINV_FA, "Invalid RTC forwarded answer")                            \
    XX(104, ERTCDUP_FA, "Duplicate RTC forwarded answer")                          \
    XX(105, ERTCIP_FA, "WebRTC forwarded ip differs")                              \
    XX(106, ERTCDISCARD_FA, "Invalid discarding of expected forwarded answer")     \
    XX(107, ERTCQUOTA_FO, "RTC quota for forwarded offers exceeded")               \
    XX(109, ERTCDUP_ID, "Duplicate RTC id")                                        \
    XX(110, ERTCFAILED, "WebRTC connection failed")                                \
    XX(111, ERTCCHANNEL_ERROR, "WebRTC channel error")                             \
    XX(112, ERTCCLOSED, "WebRTC connection was closed")                            \
    XX(113, ERTCTEXT, "Text over RTC is not supported")                            \
    XX(114, ERTCUNIQUEIP, "WebRTC SDP ip is not unique")                           \
    XX(115, ERTCUNIQUEIP_RFA, "WebRTC SDP ip not unique (forward answer)")         \
    XX(116, ERTCWRONGIP_RFA, "Wrong WebRTC SDP ip (forward answer)")               \
    XX(117, ERTCWRONGIP_FO, "Wrong WebRTC SDP ip (forward offer)")                 \
    XX(118, ERTCUNIQUEIP_RFO, "WebRTC forward offer request SDP ip is not unique") \
    XX(119, ERTCUNVERIFIEDIP, "WebRTC ip not verified")                            \
    XX(120, ERTCDUP_DATACHANNEL, "WebRTC ip not verified")                         \
    XX(121, ERTCUNEXP_VA, "Unexpected WebRTC verification answer")                 \
    XX(122, EADDRNOTFOUND, "Address not found")                                    \
    XX(123, EACCIDNOTFOUND, "Account id not found")                                \
    XX(124, ECONNRATELIMIT, "Connection rate limit exceeded")                      \
    XX(125, EINITV1, "Init V1 not allowed from this peer")                         \
    XX(126, EINITV3, "Init V3 not allowed from this peer")                         \
    XX(127, EFROZENACC, "Account is frozen and can't send")                        \
    XX(128, EHEADERRANGE, "Invalid header range")                                  \
    XX(129, EBADPRICE, "Invalid price")                                            \
    XX(130, EBADFEE, "Invalid fee")                                                \
    XX(131, ETOKIDNOTFOUND, "Token id not found")                                  \
    XX(132, ENOPOOL, "No pool liquidity")                                          \
    XX(133, ERTCDISABLED, "WebRTC disabled, cannot receive message")               \
    XX(134, EIDPOLICY, "Block transaction id policy violated")                     \
    XX(179, EINV_WART, "Invalid WART amount")                                      \
    XX(180, EINV_SUPPLY, "Invalid asset supply")                                   \
    XX(181, ETXTYPESTATE, "Transaction type invalid at current chain state")       \
    XX(182, EASSETNAME, "Invalid asset name")                                      \
    XX(183, EZEROBASEQUOTE, "Base and quote can't be both zero")                   \
    XX(184, ETOKBALANCE, "Insufficient token balance")                             \
    XX(185, EASSETHASHNOTFOUND, "Asset hash not found")                            \
    XX(186, EASSETIDNOTFOUND, "Asset id not found")                                \
    XX(187, ECANCELSELF, "Cannot cancel future transactions")                      \
    XX(188, ECANCELFUTURE, "Cannot cancel future transactions")                    \
    XX(189, ETXTYPE, "Invalid transaction message type")                           \
    XX(190, EEXCESSBYTES, "Excessive bytes after parsing")                         \
    XX(191, EWARTTOKID, "Illegal use of WART in non-WART token id")                \
    XX(192, EBLOCKV4, "Cannot use BlockV4 features")                               \
    XX(193, EZEROWART, "WART amount cannot be 0")                                  \
    XX(194, EPOOLREDEEM, "Cannot redeem more shares than pool issued")             \
    XX(195, EINVBUY, "Invalid buy value")                                          \
    XX(196, ETOKENDECIMALS, "Invalid token decimals")                              \
    XX(197, EINVSIG, "Invalid signature")                                          \
    XX(198, EBATCHSIZE2, "Invalid batch size")                                     \
    XX(199, EBATCHSIZE3, "Invalid batch size")                                     \
    /*200 - 299: Errors not leading to ban*/                                       \
    XX(200, ERTCNOSIGNAL, "WebRTC signaling server was closed (offer)")            \
    XX(201, ERTCNOSIGNAL2, "WebRTC signaling server was closed (answer)")          \
    XX(202, EMSGINTEGRITY0, "Message integrity check 0 failed")                       \
    XX(203, EMSGINTEGRITY1, "Message integrity check 1 failed")                       \
    XX(204, EMSGINTEGRITY2, "Message integrity check 2 failed")                       \
    XX(206, ESTARTWEBSOCK, "Error while starting websocket connection")            \
    XX(207, EWEBSOCK, "Error while in websocket connection")                       \
    XX(208, ERTCOWNIP, "Own announced IP not present in ")                         \
    XX(209, ERTCFWDREJECT, "WebRTC forward rejected")                              \
    XX(210, ERTCCHANNEL_CLOSED, "WebRTC datachannel closed")                       \
    XX(211, ERTCNOPEER, "WebRTC verification peer already closed")                 \
    XX(212, ERTCNOIP, "Cannot select own WebRTC ip")                               \
    XX(213, ERTCIDIP, "IP in verification offer is not announced as identity")     \
    XX(214, ERTCFEELER, "Normal feeler connection shutdown")                       \
    XX(216, EAPICMD, "Triggered by API command")                                   \
    XX(217, EMINFEE, "Transaction fee below threshold")                            \
    /*300 - 399: API triggered errors*/                                            \
    XX(300, EINV_HEX, "Cannot parse hexadecimal input")                            \
    XX(301, EBADNONCE, "Cannot parse nonce")                                       \
    XX(302, EINEXACTFEE, "Inexact fee not allowed")                                \
    XX(303, EBADAMOUNT, "Invalid amount")                                          \
    XX(304, EPARSESIG, "Cannot parse signature")                                   \
    XX(305, ENOTSYNCED, "Node not synced yet")                                     \
    XX(306, EBADTOKEN, "Invalid token")                                            \
    XX(307, EINV_TOKEN, "Malformed token specification")                           \
    XX(308, ETOKENNOTFOUND, "Token not found")                                     \
    XX(309, EPARSEHASH, "Cannot parse hash")                                       \
    XX(310, EBADLIQUIDITYFLAG, "Cannot parse liquidity flag")                      \
    XX(311, EBADBUYFLAG, "Cannot parse buy flag")                                  \
    XX(312, EBADCANCELHEIGHT, "Cannot parse cancel height")                        \
    XX(313, ECANCLHNOPINH, "Cancel height must be a pin height (multiple of 32)")  \
    XX(314, EBADCANCELNONCE, "Cannot parse cancel nonce")                          \
    XX(315, EBADASSETUNITS, "Cannot parse asset units")                            \
    XX(316, EBADASSETDECIMALS, "Cannot parse asset decimals")                      \
    XX(317, EBADHEADER, "Cannot parse header")                                     \
    XX(318, EHTTPREQUEST, "HTTP request failed")                                   \
    XX(319, EINVJSON, "Invalid JSON received")                                     \
    XX(320, EINVINTERVAL, "Invalid candle interval")                               \
    XX(321, EINVARGCOMB, "Invalid argument combination")                           \
    XX(322, ERANGETOOBIG, "Specified result range too big")                        \
    XX(323, EINVRANGE, "Specified range invalid")                                  \
    XX(324, EAPINOTENABLED, "API method not enabled")                          \
    XX(325, EHASHRATEINTERVAL, "Hashrate estimate needs at least 2 block times")   \
    XX(1000, ESIGTERM, "Received SIGTERM")                                         \
    XX(1001, ESIGHUP, "Received SIGHUP")                                           \
    XX(1002, ESIGINT, "Received SIGINT")                                           \
    XX(1003, EREFUSED, "Connection refused due to ban")                            \
    XX(1005, EDUPLICATECONNECTION, "Duplicate connection")                         \
    XX(1006, EEVICTED, "Connection was evicted")                                   \
    XX(2000, EBUG, "Bug-related error")

#define ERR_DEFINE(code, name, _) constexpr int32_t name = code;
ADDITIONAL_ERRNO_MAP(ERR_DEFINE)
#undef ERR_DEFINE
