/**
 * peer.h
 *
 * This file aims to implement a DHash peer, which should be capable of
 * put and get operations, maintenance functions, stabilization, and running
 * a server to respond to requests from other nodes.
 */

#ifndef CHORD_FINAL_PEER_H
#define CHORD_FINAL_PEER_H
#define NUM_REPLICAS 14

#include <boost/uuid/uuid.hpp>
#include <string>
#include <json/json.h>
#include <vector>
#include <map>
#include "peer_repr.h"
#include "finger_table.h"
#include "server.h"
#include "client.h"
#include "database.h"
#include "data_block.h"

/**
 * The class "Peer" represents a locally-run peer in a P2P system.
 * It refers specifically to a peer being run on this machine,
 * as opposed to "PeerRepr" (the base class) which represents
 * any peer in the chord.
 * An instance of "Peer" should run three threads:
 *    - A client thread, which makes requests to other peers.
 *    - A server thread, which responds to requests from other peers.
 *    - A stabilization thread, which updates finger table entries.
 */
class Peer : public PeerRepr {
public:
    /// Typedef denoting a map of keys to string values.
    typedef std::map<Key, std::string> KeyValueStore;

    /// Typedef denoting a member func which takes a JSON request and yields a JSON response.
    typedef std::_Mem_fn<Json::Value (Peer::*)(const Json::Value &)> RequestHandler;

    /**
     * Construct peer at [IP_ADDR]:[PORT].
     *
     * @param ip_addr IP address of
     * @param port
     */
    Peer(const char *ip_addr, int port);

    /**
     * Simple destructor for peer to free ptrs.
     */
    void Destroy();

    /**
     * Initialize chord as the first peer in said chord.
     *
     * @return True for success, false for failure.
     */
    bool StartChord();

    /**
     * Join the chord through a gateway peer.
     *
     * @param gateway_ip IP of gateway peer.
     * @param port Port of gateway peer.
     * @return True for success, false for failure.
     */
    bool Join(const char *gateway_ip, int port);

    /**
     * Leave chord.
     *
     * @return True for success, false for failure.
     */
    bool Leave();

    /**
     * Create new KV pair, either locally or otherwise.
     *
     * @param key Hashed key of new KV pair.
     * @param value Value of new KV pair.
     * @return True for success, false for failure.
     */
    bool Create(const Key &key, const std::string &value);

    /**
     * Read the value of a KV pair given the key.
     *
     * @param key Hashed key of KV pair.
     * @return V of KV pair with key [key].
     */
    DataBlock Read(const Key &key);

private:
	/// Mapping of keys to fragments.
    Database database_;

    /// The peer directly preceding this one in the chord ring.
    std::optional<PeerRepr> predecessor_;

    /// The peers directly succeeding this one in the chord ring.
    PeerList successors_;

    /// Maps keys to their successors to aid lookups.
    FingerTable *finger_table_;

    /// Server to be run locally.
    Server<RequestHandler, Peer> *server_;

    /// Makes requests to servers of other peers.
    Client *client_;

	/// ID of the peer currently connected to our server.
	std::optional<Key> current_client_id_;

	/// Thread that runs maintenance in the background.
    std::thread maintenance_thread_;

	/**
	 * Output formatted text to terminal.
	 * @param str String to format.
	 */
	void Log(const std::string &str);

	/**
	 * Send request to the given peer.
	 *
	 * @param request Request to send.
	 * @param peer Peer to send it to.
	 * @return Response from peer.
	 */
	Json::Value MakeRequest(Json::Value request, const PeerRepr &peer);

	/**
	 * Log certain peer as our current client, make sure that peer is who
	 * they claim to be before answering request and that we are intended
	 * recipient.
	 * @param request Request to validate.
	 * @return Is request valid?
	 */
	bool ValidateRequest(const Json::Value &request);

    /**
     * Is key owned on this peer?
     * I.e. Is this peer the immediate successor of the key?
     *
     * @param key Key to look for.
     * @return Is this->min_key_ <= key <= this->id_;
     */
    bool OwnedLocally(const Key &key);

    /**
     * Is key stored on this peer?
     * I.e. Is this peer one of the NUM_REPLICAS successors of the key?
     *
     * @param key Key to look for.
     * @return Is this->min_key_ <= key <= this->max_key_?
     */
    bool StoredLocally(const Key &key);

    /**
     * When given a request pertaining to a certain key, forward
     * said request to the corresponding peer in the finger table.
     *
     * @param request Request to forward
     * @param key The key to which the request corresponds, which
     *            will be queried in the finger table.
     * @return The response given by the relevant peer.
     */
    Json::Value ForwardRequest(const Json::Value &request, const Key &key);

    /// Under ideal conditions, the nth fragment of a given key is stored on
    /// the nth successor of that key. The immediate successor of that key
    /// will receive requests to CRUD keys and will do so by put/get-ing fragments
    /// stored on its successors. These functions issue and handle these
    /// requests for each CRUD op.
    bool CreateFragment(const PeerRepr &recipient, const Key &key,
                        const DataFragment &fragment);
    Json::Value CreateFragmentHandler(const Json::Value &request);
	DataFragment ReadFragment(const PeerRepr &recipient, const Key &key);
    Json::Value ReadFragmentHandler(const Json::Value &request);

    /**
     * Return a representation of the peer which succeeds [key].
     *
     * @param key The hashed key in question.
     * @return A representation of the peer which directly succeeds it.
     */
    PeerRepr GetSuccessor(const Key &key);

	/**
	 * Retrieve the n peers succeeding a given key.
	 *
	 * @param key The key whose successors should be listed.
	 * @param n The number of successors in the vector.
	 * @return A vector of size n containing the n successors of key.
	 */
	std::vector<PeerRepr> GetNSuccessors(const Key &key, int n);

    /**
     * Return a representation of the peer which precedes [key].
     * Key may refer to either a key or the id of a peer.
     *
     * @param key The hashed key or the ID of the peer in question.
     * @return A representation of the peer which directly precedes the key/peer.
     */
    PeerRepr GetPredecessor(const Key &key);

	/**
	 * Get N predecessors of key.
	 * @param key Key whose predecessors will be found.
	 * @param n Num predecessors to get.
	 * @return Vector of predecessors.
	 */
	std::vector<PeerRepr> GetNPredecessors(const Key &key, int n);

    /**
     * Interpret a peer's request to join.
     *
     * @param request JSON request sent by peer requesting to join.
     * @return Response to be sent to requesting peer.
     */
    Json::Value JoinHandler(const Json::Value &request);

    /**
     * Given a JSON request asking to heave, handle this leave.
     *
     *
     * @param request A JSON value asking to leave
     * @return A JSON value specifying keys to absorb, among other things.
     */
    Json::Value LeaveHandler(const Json::Value &request);

	/**
	 * Kill the server, delete all keys, stop running peer.
	 * Also used to simulate "failure" in unit tests.
	 */
	void Kill();

    /**
     * Handle a request to identify a key's successor.
     *
     * @param request A request specifying a key.
     * @return A response indicating its successor.
     */
    Json::Value GetSuccHandler(const Json::Value &request);

    /**
     * Handle a request to identify a key's predecessor.
     *
     * @param request A request specifying a key.
     * @return A response indicating its predecessor.
     */
    Json::Value GetPredHandler(const Json::Value &request);

    /**
     * Assess validity of finger table entries and successor list.
     */
    void Stabilize();

	/**
	 * Stabilize, run local maintenance, run global maintenance.
	 */
	void RunGeneralMaintenance();

	/**
	 * If another peer tells us that we need to run maintenance (could happen
	 * in a variety of circumstances), do so.
	 * @param request Request instructing us to run maintenance.
	 * @return Response indicating success.
	 */
	Json::Value RunGeneralMaintenanceHandler(const Json::Value &request);

	/**
	 * Determine if a given key->frag pair ought to be stored on this peer,
	 * otherwise forward to another peer.
	 */
	void RunGlobalMaintenance();

	/**
	 * Conversely, local maintenance distributes missing fragments to successors
	 * who do not hold the relevant fragments.
	 */
	void RunLocalMaintenance();

	/**
     * Ensure that the given successor has all of the same keys that we do
     * within a given range. If we have a key it does not within that range,
     * then tell them as much.
     *
     * @param succ Successor with which to synchronize.
     * @param lower_bound Lower bound of range of keys to synchronize.
     * @param upper_bound Upper bound of range of keys to synchronize.
     */
    void Synchronize(const PeerRepr &succ, const Key &lower_bound,
                     const Key &upper_bound);

	/**
	 * When told to synchronize a range of keys with a predecessor, do so.
	 *
	 * @param request Request specifying range to synchronize.
	 * @return Response specifying success.
	 */
	Json::Value SynchronizeHandler(const Json::Value &request);

    /**
     * When a key is determined to be missing, reconstruct the data block
     * and input a random fragment from the block into our database.
     *
     * @param key A key that is missing.
     */
	void RetrieveMissing(const Key &key);

//  Methods to implement in future to make use of merkle trees.
//  void CompareNodes(const CSMerkleNode &local_node, std::vector<int> dirs);
//	CSMerkleNode ExchangeNode(const CSMerkleNode &local_node);
//	Json::Value ExchangeNodeHandler(const Json::Value &request);

	/**
	 * Notify "peer_to_notify" peers about the entry of "new_peer" to the chord.
	 *
	 * @param new_peer Info regarding new peer that has entered chord.
	 * @return True on success, false on failure.
	 */
	bool Notify(const PeerRepr &new_peer, const PeerRepr &peer_to_notify);

	/**
	 * Handle notification that new peer has entered chord.
	 *
	 * @param request Request informing this peer of a new peer.
	 */
	Json::Value NotifyHandler(const Json::Value &request);

	/**
	 * Get successors of all appropriate finger table ranges.
	 *
	 * @param initialize Are we updating or creating table entries?
	 */
	void PopulateFingerTable(bool initialize);
};

#endif
