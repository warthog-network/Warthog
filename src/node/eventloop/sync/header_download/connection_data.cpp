#include "connection_data.hpp"
#include "header_download.hpp"

namespace HeaderDownload{
    ConnectionData::ConnectionData(Downloader& d)
        : leaderIter(d.leaders_end()) {};
}
