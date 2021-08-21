/**
 * database.h
 *
 * This file aims to implement a simple database class, consisting of a merkle
 * tree index (facilitating quick synchronization of ranges between local and
 * remote databases) and a map of keys to values.
 *
 * TO DO:
 *      - Make persistent.
 */

#ifndef CHORD_FINAL_DATABASE_H
#define CHORD_FINAL_DATABASE_H

#include "data_block.h"
#include "merkle_node.h"

typedef std::map<Key, DataFragment> KeyFragMap;
typedef std::pair<Key, DataFragment> KeyFragPair;

class Database {
public:
	/**
	 * Constructor, initialize merkel tree root.
	 */
	Database();

	/**
	 * Destructor - call manual destruct for merkel tree root.
	 */
	~Database();

	/**
	 * Insert key fragment pair to database and index it.
	 * @param key_frag_pair {[KEY], [FRAGMENT]}
	 */
	void Insert(const std::pair<Key, DataFragment> &key_frag_pair);

	/**
	 * Return fragment corresponding to key if it exists in db, otherwise
	 * throw error.
	 *
	 * @param key Key whose fragment will be returned.
	 * @return Fragment corresonding to key.
	 */
	DataFragment Lookup(const Key &key);

	/**
	 * Update value of specified key to equal second element of KeyFragPair if
	 * it exists, otherwise throw error.
	 * @param key_frag_pair {[KEY], [FRAGMENT]}
	 */
	void Update(const KeyFragPair &key_frag_pair);

    /**
     * Delete given key from DB and index if it exists, else give error.
     * @param key Key to delete
     */
	void Delete(const Key &key);

	/**
	 * Provide the next entry after the given key if it exists, else give error.
	 * @param key Key who iterator will be incremented by one.
	 * @return Iterator corresponding to key incremented by one.
	 */
    KeyFragPair *Next(const Key &key);

	/**
	 * List all entries in data_ between lower_bound and upper_bound.
	 * @param lower_bound Lower bound of range.
	 * @param upper_bound Upper bound of range.
	 * @return Map including all keys from data_ within specified range.
	 */
    KeyFragMap ReadRange(const Key &lower_bound, const Key &upper_bound);

	/**
	 * State whether the database contains given key.
	 * @param key Key to lookup.
	 * @return Is key indexed in index_?
	 */
	bool Contains(const Key &key);

private:
	/// Key-fragment store.
    KeyFragMap data_;

	/// Index of keys held in database.
	CSMerkleNode index_;
};


#endif
