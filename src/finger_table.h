/**
 * finger_table.h
 *
 * This file aims to implement a finger table class.
 * Each peer in the chord protocol maintains a finger table mapping ranges of
 * keys to the lower bound's successors, facilitating logarithmic lookups.
 */

#ifndef CHORD_FINAL_FINGER_TABLE_H
#define CHORD_FINAL_FINGER_TABLE_H

#include <map>
#include <utility>
#include <boost/uuid/uuid.hpp>
#include "peer_repr.h"
#include "key.h"

namespace mp = boost::multiprecision;

/**
 * A finger table enables O(log(n)) lookups by mapping ranges
 * of keys/documents to the node succeeding the lower bound.
 * When seeking to CRUD a given key, a node will consult its
 * finger table, find the range containing the key, and forward
 * its request to that node. Said node will either process the req,
 * if it owns the key in question, or forward it to another node.
 */
typedef struct {
    /// Lower bound of finger's range.
    Key lower_bound_;
    /// Upper bound of finger's range.
    Key upper_bound_;
    /// Node succeeding lower bound.
    PeerRepr successor_;
} Finger;

class FingerTable {
public:
	/**
	 * Constructor.
	 * @param starting_key First table entry minus 1.
	 */
	explicit FingerTable(Key starting_key);

	/**
	 * Add a new finger to end of the table.
	 * @param finger Finger to add.
	 */
	void AddFinger(const Finger &finger);

	/**
	 * Retrieve the nth table intry.
	 * @param n Index of table entry.
	 * @return Nth table entry.
	 */
	Finger GetNthEntry(int n);

    /**
     * Iterate through fingers in the table, find the successor of a given key.
     *
     * @param key Key to lookup.
     * @return The entry in the finger table for which
     *         finger.lower_bound_ <= key <= finger.upper_bound_.
     */
    PeerRepr Lookup(const Key &key);

	/**
	 * Update the nth table entry to the given finger.
	 * @param n Entry to update.
	 * @param succ The correct successor of the given range's lower bound.
	 */
	void EditNthFinger(int n, const PeerRepr &succ);

	/**
	 * When notified of a new peer, entries in the table referring to the peer's
	 * range should be updated to point to that peer.
	 * @param new_peer Peer that recently entered system.
	 */
	void AdjustFingers(const PeerRepr &new_peer);

	/**
	 * Return the range of keys to which the nth entry in the finger table
	 * should point.
	 * @param n Index of the given range.
	 * @return ((starting_key + 2^n) mod 2^m)-((starting_key + 2^(n+1)) mod 2^m)
     *         where m is the number keys in ring.
	 */
	std::pair<Key, Key> GetNthRange(int n);

	/**
	 * Convert to string
	 * @return Table in string form.
	 */
	operator std::string();

	/**
	 * @return Is table empty?
	 */
	bool Empty();

    /// Number of entries the table should have (length of binary key ID).
    unsigned long long num_entries_;

private:
    /// The finger table itself, represented as a vector of fingers.
    std::vector<Finger> table_;

	/// First finger table entry - 1.
	Key starting_key_;

	/// Number keys in entire hash ring.
    mp::cpp_int keys_in_chord_;
};

#endif
