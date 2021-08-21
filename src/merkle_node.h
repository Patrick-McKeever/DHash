/**
 * Merkle_node.h
 *
 * This file aims to implement a compact sparse Merkle tree, a data type
 * described (w/ pseudocode) here: https://eprint.iacr.org/2018/955.pdf
 *
 * As outlined in Josh Cates' 2003 thesis, each peer in a DHash system, in
 * addition to maintaining a database of keys and values, should maintain a
 * database index through which it can easily compare its own database contents
 * with that of other nodes. The Merkle tree, a type of hash tree in which each
 * node is assigned a hash by hashing the concatenation of its childrens'
 * hashes, offers quickly lookups and easy comparison between trees
 * (i.e. if the root nodes have the same hash, the trees are identical).
 * Cates also calls for predictable lookups, a divergence from the standard
 * Merkle tree algorithm. Though he does not use the term, the data structure
 * he refers to is nearly-identical to the compact sparse Merkle tree, in which
 * new keys are inserted based on the "distance" (floor(log2(key1 ^ key2)))
 * between the new key and the left and right branches of any given node.
 *
 *
 * This file should accomplish as follows:
 *      - Implement a class "CSMerkleNode", an implementation of the compact
 *        sparse Merkle node algorithm referenced above;
 *      - Implement a method for key lookups within that class;
 *      - Implement a method for predictable key insertions within that class.
 *
 * The use of this "CSMerkleNode" class as a database index on DHash peers will
 * enable database synchronization.
 */

#ifndef CHORD_FINAL_MERKLE_TREE_H
#define CHORD_FINAL_MERKLE_TREE_H

#include <utility>
#include <cmath>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <json/json.h>
#include "key.h"

namespace mp = boost::multiprecision;

/**
 * Implementation of a compact sparse Merkle node.
 */
class CSMerkleNode {
public:
    /// "hash_" refers to the hash of a node.
    /// "max_key_" refers to the max_key held under a given node.
    /// The latter is not presently used, but will be if I implement
    /// a full Merkle-proof method in the future.
    Key max_key_, hash_;

    /// Pointers to the left child, right child, and parent of a given node,
    /// respectively. "root_" holds the information for the node itself,
    /// to facilitate forwarding to the private insert function.
    CSMerkleNode *left_ = nullptr, *right_ = nullptr, *root_ = nullptr;

    /**
     * Constructor 1. Construct a node with no children and the input hash.
     *
     * @param key Key (i.e. hash value) of the new node.
     */
    explicit CSMerkleNode(Key key);

    /**
     * Constructor 2. Construct from left child and right child.
     *
     * @param left Left child of new node.
     * @param right Right child of new node.
     */
    CSMerkleNode(CSMerkleNode *left, CSMerkleNode *right);

    /**
     * Constructor 3. Construct from JSON.
     *
     * @param json_node JSON-encoded version of node.
     */
    explicit CSMerkleNode(const Json::Value &json_node);

    /**
     * Delete the pointer members of the instance in question.
     * A traditional destructor is called at inopportune times,
     * so it is prudent to make destruction available only by
     * manual invocation.
     */
    void Destruct() const;

    /**
     * Public insert. Insert a key into the root node.
     *
     * @param key
     */
    void Insert(const Key &key);

    /**
     * Delete the given key from the Merkle tree while
     * @param key
     */
    void Delete(const Key &key);

    /**
     * Public conains. See if root node contains a given key.
     *
     * @param key The key to search for within this tree.
     * @return Whether or not said key exists within tree.
     */
    [[nodiscard]] bool Contains(const Key &key) const;

    /**
     * Return node at specified position.
     *
     * @param directions Vector of 0s (for lefts) and 1s (for rights) denoting
     *                   the position of the node in question.
     * @return The node specified by arguments.
     */
    CSMerkleNode GetPosition(const std::vector<int> &directions);

    /**
     * Typecast to string.
     * @return String of form:
     *              "HASH: [HASH]"
     *             "LEFT: {"
     *              "   [LEFT_CHILD_STRING]"
     *              "}"
     *              "RIGHT: {"
     *              "   [RIGHT_CHILD_STRING]"
     *              "}"
     */
    explicit operator std::string();

    /**
     * Convert to JSON.
     * @return Recursive JSON object structure with fields "left", "right",
     *         and "hash".
     */
    explicit operator Json::Value();

private:
    /**
     * Private insert. Insert key as descendant of root.
     *
     * @param root A node which will become an ancestor of key.
     * @param key The key to insert as a descendent of root.
     * @return A modified version of "root" with the key inserted.
     */
    CSMerkleNode *Insert(CSMerkleNode *root, const Key &key);

    /**
     * Delete key from root, reorder remaining leaves, return modified root.
     *
     * @param root The node from which key will be deleted.
     * @param key Key to delete.
     * @return New node with key deleted and other leaves reorganized.
     */
    CSMerkleNode *Delete(CSMerkleNode *root, const Key &key);

    /**
     * Take a leaf node, place it and key as children of new node, return
     * new node.
     *
     * @param leaf Leaf which will be sibling of key.
     * @param key Key to insert as sibling of leaf.
     * @return Pointer to the parent of leaf and key.
     */
    static CSMerkleNode *InsertLeaf(CSMerkleNode *leaf, const Key &key);


    /**
     * Private "Contains". Test if root contains key.
     *
     * @param root The node whose successors will be checked.
     * @param key The key to check for.
     * @return Is key a descendant of root?
     */
    bool Contains(CSMerkleNode *root, const Key &key) const;

    /**
     * @return Is the node in question a leaf?
     */
    [[nodiscard]] bool IsLeaf() const;

    /**
     * Private string converter. Convert node to string.
     *
     * @param level Level of recursion at which function was called (0 for root
     *              node).
     * @return Recurisvely-generated string representing node and its children,
     *         in form:
     *              "HASH: [HASH]"
     *              "LEFT: {"
     *              "   [LEFT_CHILD_STRING]"
     *              "}"
     *              "RIGHT: {"
     *              "   [RIGHT_CHILD_STRING]"
     *              "}"
     */
    [[nodiscard]] std::string ToString(int level) const;
};

/// Because log2 requires this type, and because this type has only one
/// application in this file, I will refer to the type based on this single
/// application: representing the distance between two keys.
typedef mp::cpp_bin_float_100 KeyDist;

/**
 * Return the distance between two keys.
 *
 * @param key1 First key.
 * @param key2 Second key.
 * @return floor(log_base2(key1 ^ key2))
 */
static KeyDist Distance(const Key &key1, const Key &key2)
{
    mp::uint256_t xor_keys = mp::uint256_t(key1) ^ mp::uint256_t(key2);
    return floor(log2(mp::cpp_bin_float_100(xor_keys)));
}

/**
 * Concatenate hash representations of two keys, hash the result.
 *
 * @param key1 First key.
 * @param key2 Second key.
 * @return Hash of string(first key) + string(second key).
 */
static Key ConcatHash(const Key &key1, const Key &key2)
{
    return Key(std::string(key1) + std::string(key2), false);
}

#endif