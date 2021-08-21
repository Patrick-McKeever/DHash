#include "peer.h"
#include <random>
#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

/* ----------------------------------------------------------------------------
 * CONSTRUCTORS/MISC: Implement peer constructors and miscellaneous.
 * -------------------------------------------------------------------------- */

Peer::Peer(const char *ip_addr, int port)
// C++ requires you initialize the base class in the member init list.
        : PeerRepr(Key(std::string(ip_addr) + ":" + std::to_string(port), false),
                   Key(std::string(ip_addr) + ":" + std::to_string(port), false),
                   Key(std::string(ip_addr) + ":" + std::to_string(port), false),
                   ip_addr, port)
        , successors_(NUM_REPLICAS)
{
    Log("Creating new node with id " + std::string(id_));
    finger_table_ = new FingerTable(id_);

    std::map<std::string, RequestHandler> commands {
            { "JOIN", std::mem_fn(&Peer::JoinHandler) },
            { "GET_SUCC", std::mem_fn(&Peer::GetSuccHandler) },
            { "GET_PRED", std::mem_fn(&Peer::GetPredHandler) },
            { "CREATE_FRAG", std::mem_fn(&Peer::CreateFragmentHandler) },
            { "READ_FRAG", std::mem_fn(&Peer::ReadFragmentHandler) },
            { "LEAVE", std::mem_fn(&Peer::LeaveHandler) },
            { "NOTIFY", std::mem_fn(&Peer::NotifyHandler) },
            { "READ_FRAG", std::mem_fn(&Peer::ReadFragmentHandler) },
            { "SYNCHRONIZE", std::mem_fn(&Peer::SynchronizeHandler) },
            { "MAINTENANCE", std::mem_fn(&Peer::RunGeneralMaintenanceHandler) }
    };

    server_ = new Server(port_, commands, this);
    client_ = new Client;
}

void Peer::Destroy()
{
    if(maintenance_thread_.joinable())
        maintenance_thread_.join();
    else
        Log("Not joinable");

    Log("FINAL RANGE: " + std::string(min_key_) + " - " + std::string(id_));
    Log("PREDECESSOR: " + (predecessor_.has_value() ?
                           (std::string(predecessor_->id_) + " at " +
                            predecessor_->ip_addr_ + ":" + std::to_string(predecessor_->port_)) :
                           "NONE"));

    if(successors_.Size() == 0)
        Log("SUCCESSORS: NONE");
    else {
        Log("SUCCESSORS:");
        for(int i = 0; i < successors_.Size(); i++)
            std::cout << "\t\t\t" << std::string(successors_.GetNthEntry(i).id_)
                      << std::endl;
    }

    Log("FINAL FINGER TABLE:\n" + std::string(*finger_table_));
}

void Peer::Log(const std::string &str)
{
    std::cout << "[" << std::string(id_) << " " << port_ << "] " << str
              << std::endl;
}

bool Peer::OwnedLocally(const Key &key)
{
    // Note that this is distinct from simply holding a key.
    // DHash replication requires that N successors of a key hold it,
    // so any node will hold its predecessor's keys.
    // Ownership of a key by a peer implies that said peer is the *immediate*
    // successor of the key in question, i.e. the key is between the peer's
    // predecessor and itself.
    if(predecessor_.has_value())
        return key.InBetween(predecessor_.value().id_ + 1, id_, true);

    return true;
}

bool Peer::StoredLocally(const Key &key)
{
    return key.InBetween(min_key_, id_, true);
}


/* ----------------------------------------------------------------------------
 * NETWORKING INTERFACE: Implement functions allowing peers to messsage other
 						 peers and to log/assess the validity of those
 						 requests.
 * -------------------------------------------------------------------------- */

Json::Value Peer::MakeRequest(Json::Value request, const PeerRepr &peer)
{
    request["SENDER_ID"] = std::string(id_);
    request["RECIPIENT_ID"] = std::string(peer.id_);
    try {
        return client_->MakeRequest(peer.ip_addr_, peer.port_, request);
    } catch(...) {
        throw std::exception();
    }
}

bool Peer::ValidateRequest(const Json::Value &request)
{
    if(request["RECIPIENT_ID"].asString() != std::string(id_))
        return false;

    current_client_id_ = Key(request["SENDER_ID"].asString(), true);
    return true;
}

Json::Value Peer::ForwardRequest(const Json::Value &request, const Key &key) {
    PeerRepr key_succ = finger_table_->Lookup(key);
    bool key_succ_is_busy = key_succ.id_ == current_client_id_,
            key_succ_is_us = key_succ.id_ == id_;

    if(key_succ_is_busy || key_succ_is_us) {
        if(current_client_id_ == predecessor_->id_)
            return MakeRequest(request, successors_.GetNthEntry(0));
        else
            return MakeRequest(request, predecessor_.value());
    }

    try {
        return MakeRequest(request, key_succ);
    } catch(...) {
        throw std::exception();
    }
}


/* ----------------------------------------------------------------------------
 * JOIN/LEAVE: Implement functions for peers to start a chord, join it,
 *			   leave it gracefully, or simply exit without notification,
 *			   as well as the necessary handlers for these events. Also
 *			   implement the necessary notification functions by which new
 *			   peers alert succs/preds to their presence, as well as their
 *			   handlers.
 * -------------------------------------------------------------------------- */

bool Peer::StartChord()
{
    Log("Starting chord");
    try {
        // If this peer is the only peer in ring, then this peer owns all keys.
        // Hence, its range will be [id_ + 1, id_], covering the whole of the ring.
        min_key_ = id_ + 1;

        // Run server as daemon.
        server_->RunInBackground();

        // Prevent race condition.
        std::this_thread::sleep_for(10ms);

        maintenance_thread_ = std::thread([this] {
          sleep(5);
          RunGeneralMaintenance();
        });
        maintenance_thread_.detach();

        return true;
    } catch (...) {
        return false;
    }
}

bool Peer::Join(const char *gateway_ip, int port)
{
    Log("Joining chord");
    // Run server as daemon.
    server_->RunInBackground();
    std::this_thread::sleep_for(10ms);

    Json::Value join_req;
    join_req["COMMAND"] = "JOIN";
    PeerRepr *this_peer = this;
    join_req["NEW_PEER"] = Json::Value(*this_peer);
    Json::Value join_resp = client_->MakeRequest(gateway_ip, port, join_req);
    predecessor_ = PeerRepr(join_resp["PREDECESSOR"]);
    min_key_ = predecessor_->id_ + 1;
    Log("Predecessor given by gateway is " + std::string(predecessor_->id_));
    Log("New range is " + std::string(min_key_) + "-" + std::string(id_));

    PopulateFingerTable(true);
    Log("CURRENT RANGE: " + std::string(min_key_) + "-" + std::string(id_));
    Log("FINGER TABLE INITIALIZED AS:\n" + std::string(*finger_table_));

    // Notify all 14 predecessors so they can update their successor lists.
    for(const PeerRepr &pred : GetNPredecessors(id_, NUM_REPLICAS))
        Notify(*this_peer, pred);

    successors_ = PeerList(NUM_REPLICAS, GetNSuccessors(id_, NUM_REPLICAS));
    Notify(*this_peer, successors_.GetNthEntry(0));
}

Json::Value Peer::JoinHandler(const Json::Value &request)
{
    Log("Here");
    Json::Value join_resp;
    PeerRepr new_peer(request["NEW_PEER"]);

    // Get the predecessor and successor of new peer, store in JSON.
    PeerRepr new_peer_pred = GetPredecessor(new_peer.id_);

    join_resp["PREDECESSOR"] = Json::Value(new_peer_pred);
    Log("RESPONDING TO JOIN RESP WITH " + join_resp.toStyledString());
    return join_resp;
}

bool Peer::Leave()
{
    Json::Value notification_for_succ, notification_for_pred;
    // Our predecessor becomes our successor's predecessor.
    notification_for_succ["COMMAND"] = "LEAVE";
    notification_for_succ["NEW_PRED"] = Json::Value(*predecessor_);
    notification_for_succ["NEW_MIN"] = std::string(min_key_ + 1);

    // Allow predecessor to update its finger table entries to account for our
    // absence.
    PeerRepr succ = successors_.GetNthEntry(0);
    succ.min_key_ = min_key_;
    notification_for_pred["COMMAND"] = "LEAVE";
    notification_for_pred["NEW_SUCC"] = Json::Value(succ);

    MakeRequest(notification_for_succ, successors_.GetNthEntry(0));
    MakeRequest(notification_for_pred, *predecessor_);

    Kill();
}

Json::Value Peer::LeaveHandler(const Json::Value &request)
{
    ValidateRequest(request);
    Json::Value json_resp;

    if(current_client_id_ == predecessor_->id_) {
        predecessor_ = PeerRepr(request["NEW_PRED"]);
        min_key_ = Key(request["NEW_MIN"].asString(), true);
    }

    if(current_client_id_ == successors_.GetNthEntry(0).id_)
        finger_table_->AdjustFingers(request["NEW_SUCC"]);

    current_client_id_.reset();
    return json_resp;
}

void Peer::Kill()
{
    // This would be equivalent to an un-graceful leave.
    server_->Kill();
}

bool Peer::Notify(const PeerRepr &new_peer, const PeerRepr &peer_to_notify)
{
    Log("Sending notification to " + std::to_string(peer_to_notify.port_));
    Json::Value notif_req;
    notif_req["COMMAND"] = "NOTIFY";
    // ID of peer receiving the request.
    notif_req["RECIP_ID"] = std::string(peer_to_notify.id_);
    // Information of the new peer.
    notif_req["NEW_PEER"] = Json::Value(new_peer);

    Json::Value notif_resp = client_->MakeRequest(peer_to_notify.ip_addr_,
                                                  peer_to_notify.port_,
                                                  notif_req);
    return notif_resp["SUCCESS"].asBool();
}

Json::Value Peer::NotifyHandler(const Json::Value &request)
{
    Json::Value notify_resp;
    Key recipient_id(request["RECIP_ID"].asString(), true);
    PeerRepr new_peer(request["NEW_PEER"]);

    // If the new peer is clockwise-between the current predecessor and
    // this peer, then the new peer will replace the current predecessor.
    // If we don't have a predecessor, then this key can be assumed as our pred.
    // This ternary ensures that the second condition is not evaluate if
    // predecessor is nullptr (which would cause segfault.
    bool peer_is_pred = ! predecessor_.has_value() ||
                        new_peer.id_.InBetween(predecessor_->id_, id_, false);

    if(peer_is_pred) {
        // Update any finger tables which should now point to new peer.
        finger_table_->AdjustFingers(new_peer);
        Log("Old predecessor was " + (predecessor_.has_value() ?
                                      std::string(predecessor_->id_) :
                                      "Nothing"));
        Log("New predecessor is " + std::string(new_peer.id_));
        predecessor_ = new_peer;
        min_key_ = predecessor_->id_ + 1;
        Log("New range is " + std::string(min_key_) + "-" + std::string(id_));
        return notify_resp;
    }

    if(finger_table_->Empty())
        PopulateFingerTable(true);

    // Update any finger tables which should now point to new peer.
    finger_table_->AdjustFingers(new_peer);
    successors_.Insert(new_peer);

    return notify_resp;
}

/* ----------------------------------------------------------------------------
 * MAINTENANCE FUNCTIONS: Implement member functions which ensure that lookups,
 *                        fragment locations, etc are correct. This includes
 *                        stabilization of the finger table and successors list,
 *                        as well as global and local maintenance of fragments.
 *                        A single stabilize operation will run all of these,
 *                        while the stabilize function will run these
 *                        continuously.
 * -------------------------------------------------------------------------- */

void Peer::RunGeneralMaintenance() {
    while(successors_.Size() == 0) {}
    sleep(1);
    Log("Starting general maintenance");
    Stabilize();
    RunLocalMaintenance();
    RunGlobalMaintenance();

    Json::Value maintenance_req;
    maintenance_req["COMMAND"] = "MAINTENANCE";
    MakeRequest(maintenance_req, successors_.GetNthEntry(0));
    Log("Ending general maintenance");
}

Json::Value Peer::RunGeneralMaintenanceHandler(const Json::Value &request)
{
    maintenance_thread_ = std::thread([this] { RunGeneralMaintenance(); });
    maintenance_thread_.detach();
    Json::Value resp;
    return resp;
}

void Peer::Stabilize()
{
    Log("FINGER TABLE BEFORE STABILIZE:\n" + std::string(*finger_table_));
    PopulateFingerTable(false);

    successors_ = PeerList(NUM_REPLICAS, GetNSuccessors(id_, NUM_REPLICAS));
}

void Peer::RunGlobalMaintenance()
{
    PeerRepr *this_node = this;
    Key current_key = id_;

    do {
        std::vector<PeerRepr> succs = GetNSuccessors(current_key, NUM_REPLICAS);
        // If this node is not within the NUM_REPLICAS successors of the key,
        // key should not be stored here.
        bool key_is_misplaced = std::find(succs.begin(), succs.end(),
                                          *this_node) == succs.end();
        if (key_is_misplaced) {
            // If that key is misplaced, then the entire range of keys between
            // it and its immediate successor is misplaced.
            KeyFragMap misplaced_keys = database_.ReadRange(current_key,
                                                            succs.at(0).id_);

            for(const auto &[misplaced_key, misplaced_frag] : misplaced_keys) {
                for(const auto &succ : succs) {
                    if(CreateFragment(succ, misplaced_key, misplaced_frag)) {
                        database_.Delete(misplaced_key);
                        break;
                    }
                }
            }
        }
        current_key = succs.at(0).id_;
    } while(! current_key.InBetween(min_key_, id_, true));
    // By the time we loop back around through the entire database, we can stop.
}

void Peer::RunLocalMaintenance()
{
    for(int i = 0; i < successors_.Size(); i++)
        Synchronize(successors_.GetNthEntry(i), min_key_, id_);
}

void Peer::Synchronize(const PeerRepr &succ, const Key &lower_bound,
                       const Key &upper_bound)
{
    Json::Value synchronize_req;
    synchronize_req["COMMAND"] = "SYNCHRONIZE";
    Json::Value keys_to_synchronize;
    for(const auto &[key, _] : database_.ReadRange(lower_bound, upper_bound))
        keys_to_synchronize.append(std::string(key));
    synchronize_req["KEYS"] = keys_to_synchronize;

    // Response isn't really needed for this func.
    MakeRequest(synchronize_req, succ);
}

Json::Value Peer::SynchronizeHandler(const Json::Value &request)
{
    Log("Synchronize handler");
    Json::Value resp;
    resp["SUCCESS"] = true;
    std::vector<Key> keys_to_synchronize;
    for(const auto &it : resp["KEYS"])
        keys_to_synchronize.emplace_back(it.asString(), true);

    for(const auto &key : keys_to_synchronize)
        if(! database_.Contains(key))
            RetrieveMissing(key);

    return resp;
}

void Peer::RetrieveMissing(const Key &key)
{
    Log("Retrieving missing key " + std::string(key));
    // Reconstruct missing block, get random key from it, insert to db.
    DataBlock missing_block = Read(key);
    std::vector<DataFragment> random_els;
    std::sample(missing_block.fragments_.begin(),
                missing_block.fragments_.end(),
                std::back_inserter(random_els),
                1, std::mt19937{std::random_device{}()});

    database_.Insert({ key, random_els.at(0) });
}

void Peer::PopulateFingerTable(bool initialize)
{
    Log(std::string(initialize ? "Initializing":"Updating") + " finger table.");
    for(int i = 0; i < finger_table_->num_entries_; i++) {
        std::pair<Key, Key> entry_range = finger_table_->GetNthRange(i);

        Json::Value succ_req;
        succ_req["COMMAND"] = "GET_SUCC";
        succ_req["KEY"] = std::string(entry_range.first);

        if(initialize) {
            // Since the Peer::GetSuccessor member function depends upon
            // a populated finger table, we must instead formulate the
            // request on our own and forward it to a known node.
            // Since the first call to finger table population occurs
            // after the predecessor has been set, we forward these requests
            // to the predecessor.
            if(entry_range.first.InBetween(min_key_, id_, true)) {
                PeerRepr *this_peer = this;
                finger_table_->AddFinger(Finger { entry_range.first,
                                                  entry_range.second,
                                                  PeerRepr(*this_peer) });
            } else {
                PeerRepr peer_to_query = i == 0 ?
                                         *predecessor_ :
                                         finger_table_->GetNthEntry(i-1).successor_;
                std::string ip_to_query = peer_to_query.ip_addr_;
                Json::Value resp = MakeRequest(succ_req, peer_to_query);

                finger_table_->AddFinger(Finger { entry_range.first,
                                                  entry_range.second,
                                                  PeerRepr(resp) });
            }
        } else {
            if(i == 0)
                finger_table_->EditNthFinger(i, GetSuccessor(entry_range.first));
            else {
                PeerRepr peer_to_query = finger_table_->GetNthEntry(i-1).successor_;
                Json::Value resp;
                try {
                    resp = MakeRequest(succ_req, peer_to_query);
                } catch(...) {
                    resp = GetSuccessor(entry_range.first);
                }
                finger_table_->EditNthFinger(i, PeerRepr(resp));
            }
        }
    }
    Log("Ended finger table population.");
}

/* ----------------------------------------------------------------------------
 * SUCC/PRED FUNCTIONS: Implement member functions which retrieve successors
 						and predecessors of a given key by forwarding them
 						around the ring, as well as the relevant handlers.
 * -------------------------------------------------------------------------- */

PeerRepr Peer::GetSuccessor(const Key &key)
{
    if (key.InBetween(min_key_, id_, true)) {
        PeerRepr *local_peer = this;
        return *local_peer;
    } else {
        Json::Value get_succ_req, json_peer;
        get_succ_req["COMMAND"] = "GET_SUCC";
        get_succ_req["KEY"] = std::string(key);

        try {
            json_peer = ForwardRequest(get_succ_req, key);
        } catch(const std::exception &err) {
            json_peer = MakeRequest(get_succ_req, *predecessor_);
        }
        return PeerRepr(json_peer);
    }
}

Json::Value Peer::GetSuccHandler(const Json::Value &request)
{
    ValidateRequest(request);
    Key key(request["KEY"].asString(), true);
    PeerRepr succ = GetSuccessor(key);
    Json::Value succ_json(succ);
    succ_json["SUCCESS"] = true;

    current_client_id_.reset();

    return succ_json;
}

std::vector<PeerRepr> Peer::GetNSuccessors(const Key &key, int n)
{
    std::vector<PeerRepr> successors_list;
    Key previous_peer_id = key;

    for(int i = 0; i < n; i++) {
        PeerRepr ith_succ = GetSuccessor(previous_peer_id + 1);
        successors_list.push_back(ith_succ);

        // Imagine if this method were called with n=5 in a chord comprised
        // of only 2 peers. In this case, it would not make sense to return
        // a vector alternating between the same two peers until it reaches
        // 5 entries, so, when we "loop back around" to the first key,
        // it's time to break and return a 2-entry vector.
        if(previous_peer_id == key && i != 0) {
            Log(std::string(previous_peer_id) + " == " + std::string(key));
            break;
        }

        previous_peer_id = ith_succ.id_;
    }

    return successors_list;
}

PeerRepr Peer::GetPredecessor(const Key &key)
{
    if(! predecessor_.has_value()) {
        PeerRepr *this_peer = this;
        return *this_peer;
    }

    // If the key is stored locally, then its predecessor is this peer's predecessor.
    if (StoredLocally(key))
        return *predecessor_;
        // Otherwise, forward a request to the relevant peer.
    else {
        Json::Value get_pred_req;
        get_pred_req["COMMAND"] = "GET_PRED";
        get_pred_req["KEY"] = std::string(key);

        Json::Value json_peer = ForwardRequest(get_pred_req, key);
        return PeerRepr(json_peer);
    }
}

Json::Value Peer::GetPredHandler(const Json::Value &request)
{
    ValidateRequest(request);
    Key key(request["KEY"].asString(), true);
    PeerRepr pred = GetPredecessor(key);
    Json::Value pred_json(pred);
    pred_json["SUCCESS"] = true;
    current_client_id_.reset();

    return pred_json;
}

std::vector<PeerRepr> Peer::GetNPredecessors(const Key &key, int n)
{
    std::vector<PeerRepr> pred_list;
    Key previous_peer_id = key;

    for(int i = 0; i < n; i++) {
        PeerRepr ith_succ = GetPredecessor(previous_peer_id - 1);
        Log("Pred of " + std::string(previous_peer_id - 1) + " is " +
            std::string(ith_succ.id_));
        pred_list.push_back(ith_succ);

        // Imagine if this method were called with n=5 in a chord comprised
        // of only 2 peers. In this case, it would not make sense to return
        // a vector alternating between the same two peers until it reaches
        // 5 entries, so, when we "loop back around" to the first key,
        // it's time to break and return a 2-entry vector.
        if(previous_peer_id == key && i != 0)
            break;

        previous_peer_id = ith_succ.id_;
    }
    return pred_list;
}



/* ----------------------------------------------------------------------------
 * PUT/GET:	Due to the nature of DHash, not all CRUD operations are viable.
 *			Since no single node has a full copy of a key, the success of an
 *			update or delete depends on whether new nodes have entered the
 *			portion of the mesh in which a key resides. Consequently, old
 *			nodes which have been pushed out from that portion of the mesh
 *			may hold outdated or deleted fragments which, upon maintenance,
 *			can corrupt or overwrite up-to-date data. There is no viable
 *			fix for this, so Cates' thesis only outlines the implementation of
 *			put and get methods. Consequently, we will not implement full CRUD
 *			and handlers - only Create, Read, and the associate per-fragment
 *			operations (i.e. CreateFragment, to create a single fragment
 *			on a given node, and its handler).
 * -------------------------------------------------------------------------- */
bool Peer::Create(const Key &key, const std::string &value)
{
    // Encode value into a block comprised of data fragments.
    DataBlock block(value, true);
    int num_replicas = 0;
    std::vector<PeerRepr> succ_list = GetNSuccessors(key, NUM_REPLICAS);

    // A minimum of ten replicas are needed to reconstruct the block.
    if(succ_list.size() < 10)
        return false;

    for(int i = 0; i < block.fragments_.size(); i++) {
        if(succ_list.at(i).id_ == id_) {
            database_.Insert({ key, block.fragments_.at(i) });
            num_replicas++;
        }
        else
            num_replicas += CreateFragment(succ_list.at(i), key,
                                           block.fragments_.at(i));
    }

    // If at least 10 peers succesfully stored fragments, then the block can
    // be reconstructed by messaging them.
    return num_replicas >= 10;
}

DataBlock Peer::Read(const Key &key)
{
    std::vector<PeerRepr> succ_list = GetNSuccessors(key, NUM_REPLICAS);
    std::set<DataFragment> fragments;

    for(const auto &succ : succ_list) {
        if(fragments.size() == 10)
            break;

        if(succ.id_ == id_ && database_.Contains(key))
            fragments.insert(database_.Lookup(key));
        else {
            try {
                fragments.insert(ReadFragment(succ, key));
            }
                // If the key is not stored on the frag we message, ReadFrag
                // will throw an error. The solution is simply to message the
                // next frag.
            catch(const std::exception &err) {
                continue;
            }
        }
    }

    // A minimum of ten fragments are needed to reconstruct a data block.
    if(fragments.size() < 10)
        throw std::runtime_error("Less than 10 distinct frags.");

    return DataBlock(std::vector<DataFragment>(fragments.begin(),
                                               fragments.end()));
}

bool Peer::CreateFragment(const PeerRepr &recipient, const Key &key,
                          const DataFragment& fragment)
{
    if(recipient.id_ == current_client_id_ || recipient.id_ == id_)
        return false;

    Json::Value create_frag_req;
    create_frag_req["COMMAND"] = "CREATE_FRAG";
    create_frag_req["KEY"] = std::string(key);
    create_frag_req["FRAGMENT"] = std::string(fragment);

    Json::Value create_frag_resp = MakeRequest(create_frag_req, recipient);
    return create_frag_resp["SUCCESS"].asBool();
}

Json::Value Peer::CreateFragmentHandler(const Json::Value &request)
{
    ValidateRequest(request);
    Log("Creating Fragment");
    Json::Value resp;

    Key key(request["KEY"].asString(), true);

    if(database_.Contains(key))
        throw std::runtime_error("Key already in db.");

    DataFragment frag(request["FRAGMENT"].asString());
    database_.Insert({ key, frag });
    current_client_id_.reset();
    return resp;
}

DataFragment Peer::ReadFragment(const PeerRepr &recipient, const Key &key)
{
    Json::Value read_frag_req;
    read_frag_req["COMMAND"] = "READ_FRAG";
    read_frag_req["KEY"] = std::string(key);

    Json::Value read_frag_resp = MakeRequest(read_frag_req, recipient);
    if(read_frag_resp["SUCCESS"].asBool())
        return DataFragment(read_frag_resp["FRAGMENT"].asString());
    throw std::runtime_error(read_frag_resp["ERRORS"].asString());
}

Json::Value Peer::ReadFragmentHandler(const Json::Value &request) {
    ValidateRequest(request);
    Key key(request["KEY"].asString(), true);
    Json::Value resp;
    if (database_.Contains(key)) {
        resp["FRAGMENT"] = std::string(database_.Lookup(key));
        current_client_id_.reset();
        return resp;
    }
    current_client_id_.reset();
    throw std::runtime_error("Fragment not stored locally.");
}