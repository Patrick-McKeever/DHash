/**
 * peer_repr.h
 *
 * This file exists to implement several classes:
 *   - PeerRepr, which a locally-run peer will use to represent remote peers.
 *   - PeerList, a vector wrapper used to order peers in a manner consistent
 *     with their positioning in the chord.
 * It should also implement a method to sort PeerReprs by a field "latency"
 * which represents the average time the peer takes to respond to requests.
 * TO DO:
 *   - Edit relevant client code to favor less latent peers.
 */

#ifndef CHORD_FINAL_PEER_REPR_H
#define CHORD_FINAL_PEER_REPR_H

#include <json/json.h>
#include "key.h"

/**
 * This class exists to represent peers.
 * Peers will store instances of this class to represent their successors,
 * predecessors, and finger table entries.
 * Since each peer will also have these data members, the main Peer class
 * inherits from this.
 */
class PeerRepr {
public:
    /// ID of peer (hash of its IP and port).
    Key id_;

    /// Minimum key stored by peer.
    Key min_key_;

    /// Maximum key stored by peer.
    /// Will be same as id_, but is held as separate value to improve readability.
    Key max_key_;

    /// IP address of peer.
    std::string ip_addr_;

    /// Port on which peer runs.
    int port_;

	/// Mean latency in seconds. Adjusted after each call made to peer.
	float latency_;

    /**
     * Constructor 1. Construct PeerRepr from attributes.
     *
     * @param id ID of new PeerRepr.
     * @param min_key Min key in range of keys held by new PeerRepr.
     * @param max_key Max key in range of keys held by new PeerRepr.
     * @param ip_addr IP Addr on which new PeerRepr is run.
     * @param port Port on which new PeerRepr is run.
     */
    PeerRepr(Key id, Key min_key, Key max_key, std::string ip_addr, int port);

    /**
     * Constructor 2. Construct PeerRepr from Json::Value containing data members.
     *
     * @param members Json object containing keys:
     *                      - "ID"
     *                      - "MIN_KEY"
     *                      - "MAX_KEY"
     *                      - "IP_ADDR"
     *                      - "PORT"
     */
    PeerRepr(const Json::Value &members);

	/**
	 * Convert const PeerRepr to JSON.
	 *
	 * @return JSON version of PeerRepr with members as keys.
	 */
    operator Json::Value() const;

    /**
     * Mechanism for comparing two PeerReprs.
     *
     * @param peer_repr1 Left hand peer repr.
     * @param peer_repr2 Right hand peer repr.
     * @return Are all fields the same?
     */
    friend bool operator == (const PeerRepr &peer_repr1, const PeerRepr &peer_repr2);
    friend bool operator < (const PeerRepr &peer_repr1, const PeerRepr &peer_repr2);
};

/**
 * The aim of PeerList is to create an interface for a set of peers (e.g. a
 * successor list). It should maintain a list of peers sorted by key
 */
class PeerList {
public:
	/**
	 * Constructor.
	 * @param max_entries Maximum number of entries in the peer list.
	 */
    explicit PeerList(int max_entries);

	PeerList(int max_entries, std::vector<PeerRepr> peers);

	/**
	 * Insert a new peer into the set, sorted by key
	 * @param new_peer
	 */
    bool Insert(const PeerRepr &new_peer);

    PeerRepr GetNthEntry(int n);

    unsigned long Size();

	std::vector<PeerRepr> SortByLatency();

private:
    int max_entries_;
    std::vector<PeerRepr> peers_;
};


/**
 * This struct facilitates the sorting of peers by latency.
 */
struct LatencySort {
	/**
	 * Less than operator. Given to peers, return whether the lefthand
	 * has lower latency than the right hand.
	 *
	 * @param peer1 Letfhand.
	 * @param peer2 Righthand.
	 * @return Does lefthand have lower latency?
	 */
    inline bool operator() (const PeerRepr &peer1, const PeerRepr &peer2)
    {
        return peer1.latency_ < peer2.latency_;
    }
};

#endif
