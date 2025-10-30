#pragma once
#include <list>
#include <map>

class Header;

namespace HeaderDownload {
class Downloader;
struct LeaderNode;
using Lead_list = std::list<LeaderNode>;
using Lead_iter = Lead_list::iterator;
struct QueueBatchNode;
using Queued_iter = std::map<Header, QueueBatchNode>::iterator;
}
