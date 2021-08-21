#include "peer_repr.h"

#include <utility>

PeerRepr::PeerRepr(Key id, Key min_key, Key max_key, std::string ip_addr,
                   int port)
    : id_(std::move(id))
    , min_key_(std::move(min_key))
    , max_key_(std::move(max_key))
    , ip_addr_(std::move(ip_addr))
    , port_(port)
    , latency_(0)
{}

PeerRepr::PeerRepr(const Json::Value &members)
    : id_(members["ID"].asString(), true)
    , min_key_(members["MIN_KEY"].asString(), true)
    , max_key_(members["MAX_KEY"].asString(), true)
    , ip_addr_(members["IP_ADDR"].asString())
    , port_(members["PORT"].asInt())
    , latency_(0)
{}

PeerRepr::operator Json::Value() const
{
    // Convert peer into a json object.
    Json::Value peer_json;

    peer_json["ID"]         =   std::string(id_);
    peer_json["MIN_KEY"]    =   std::string(min_key_);
    peer_json["MAX_KEY"]    =   std::string(max_key_);
    peer_json["IP_ADDR"]    =   ip_addr_;
    peer_json["PORT"]       =   port_;

    return peer_json;
}

bool operator == (const PeerRepr &peer_repr1, const PeerRepr &peer_repr2)
{
    return (peer_repr1.ip_addr_ == peer_repr2.ip_addr_ &&
            peer_repr1.id_      == peer_repr2.id_      &&
            peer_repr1.max_key_ == peer_repr2.max_key_ &&
            peer_repr1.min_key_ == peer_repr2.min_key_ &&
            peer_repr1.port_    == peer_repr2.port_);
}

bool operator < (const PeerRepr &peer_repr1, const PeerRepr &peer_repr2)
{
	return peer_repr1.id_ < peer_repr2.id_;
}

PeerList::PeerList(int max_entries)
    : max_entries_(max_entries)
{}

PeerList::PeerList(int max_entries, std::vector<PeerRepr> peers)
    : max_entries_(max_entries)
    , peers_(std::move(peers))
{}

bool PeerList::Insert(const PeerRepr &new_peer)
{
	// In case you're wondering whether we could use std::set instead of a
	// vector, we can't. The sorting requires comparison of each element
	// to both left and right entries in each prospective position, since it
	// uses clockwise in-between. As a result, we need to use a vector and
	// implement our own insert.
	if(peers_.empty()) {
		peers_.push_back(new_peer);
		return true;
    }

    // Since we'll be comparing successors to the entries and ids before them,
    // we need to initialize these variables here.
	Key previous_key = peers_.back().id_;
    // Does new peer belong on successors list?
	bool new_peer_is_succ = false;
    // Position of the new peer in the vector (if necessary); needed for insert.
	std::vector<PeerRepr>::iterator new_peer_position;

	for(auto it = peers_.begin(); it != peers_.end(); ++it) {
		if(new_peer.id_ == it->id_)
			return false;

        if(new_peer.id_.InBetween(previous_key, it->id_, true)) {
			new_peer_position = it;
			new_peer_is_succ = true;
			break;
		}
		previous_key = it->id_;
	}

	if(new_peer_is_succ) {
        peers_.insert(new_peer_position, new_peer);
		if(peers_.size() > max_entries_)
			peers_.pop_back();
		return true;
	}

    if(peers_.size() < max_entries_) {
        peers_.push_back(new_peer);
		return true;
    }

	return false;
}

PeerRepr PeerList::GetNthEntry(int n)
{
    auto it = peers_.begin();
    std::advance(it, n);
    return *it;
}

unsigned long PeerList::Size()
{
    return peers_.size();
}

std::vector<PeerRepr> PeerList::SortByLatency()
{
    std::vector<PeerRepr> peers_by_latency = peers_;
    std::sort(peers_by_latency.begin(), peers_by_latency.end(),
              LatencySort());
	return peers_by_latency;
}