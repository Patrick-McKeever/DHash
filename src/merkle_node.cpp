#include "merkle_node.h"
#include <utility>

// Construct leaf-node w/ given hash.
CSMerkleNode::CSMerkleNode(Key key)
        : hash_(std::move(key))
        , max_key_(Key("0", true))
{}

// Construct internal/root node given children, w/ hash as hash of concatenated
// children hashes.
CSMerkleNode::CSMerkleNode(CSMerkleNode *left, CSMerkleNode *right)
        : left_(left)
        , right_(right)
        , hash_(left && right ?
                ConcatHash(left->hash_, right->hash_) :
                Key("0", true))
        , max_key_(Key("0", true))
{}

CSMerkleNode::CSMerkleNode(const Json::Value &json_node)
        : hash_(json_node["HASH"].asString(), true)
        , max_key_(Key("0", true))
{
    if(json_node.get("LEFT", NULL) != NULL)
        left_ = new CSMerkleNode(json_node["LEFT"]);
    if(json_node.get("RIGHT", NULL) != NULL)
        right_ = new CSMerkleNode(json_node["RIGHT"]);
}

void CSMerkleNode::Destruct() const
{
    delete left_;
    delete right_;
    delete root_;
}

void CSMerkleNode::Insert(const Key &key)
{
    // If this key has no children, just create new node as child.
    if(root_) {
        root_ = Insert(root_, key);
        left_ = root_->left_;
        right_ = root_->right_;
        hash_ = root_->hash_;
    } else {
        root_ = new CSMerkleNode(key);
        left_ = root_->left_;
        right_ = root_->right_;
        hash_ = root_->hash_;
    }
}

void CSMerkleNode::Delete(const Key &key)
{
    // If this key has no children, just create new node as child.
    if(root_) {
        root_ = Delete(root_, key);
        left_ = root_->left_;
        right_ = root_->right_;
        hash_ = root_->hash_;
    } else
        throw std::runtime_error("No root to delete from.");
}

bool CSMerkleNode::Contains(const Key &key) const
{
    // If root doesn't exist (i.e. node has no children), it can't contain
    // anything.
    if(root_)
        return Contains(root_, key);
    else
        return false;
}

CSMerkleNode CSMerkleNode::GetPosition(const std::vector<int> &directions)
{
    CSMerkleNode *current_node = this;
    for(const int &direction : directions) {
        if(current_node == nullptr)
            throw std::runtime_error("Node does not exist in this position");
        current_node = direction ? current_node->right_ : current_node->left_;
    }
    return *current_node;
}

CSMerkleNode::operator std::string()
{
    return ToString(0);
}

CSMerkleNode *CSMerkleNode::Insert(CSMerkleNode *root, const Key &key)
{
    // Base case for recursion. When we've been routed to a leaf, insert
    // the new node there.
    if(root->IsLeaf())
        return InsertLeaf(root, key);

    // To ensure predictable logarithmic lookups, we determine where to place
    // a node based on the "distance" (floor(log2(key1 ^ key2))) of the key
    // in question from the left and right keys.
    KeyDist l_dist = Distance(key, root->left_->hash_),
            r_dist = Distance(key, root->right_->hash_);

    // If the distance between left and right branches are equal, choose the
    // branch w/ the lower hash.
    if(l_dist == r_dist) {
        auto *new_node = new CSMerkleNode(key);
        Key min_key = left_->hash_ < right_->hash_ ? left_->hash_ :
                      right_->hash_;
        if(key < min_key)
            return new CSMerkleNode(new_node, root);
        else
            return new CSMerkleNode(root, new_node);
    }

    // Otherwise, insert in the branch with the lower distance
    if(l_dist < r_dist)
        root->left_ = Insert(root->left_, key);
    else
        root->right_ = Insert(root->right_, key);

    return new CSMerkleNode(root->left_, root->right_);
}

CSMerkleNode *CSMerkleNode::Delete(CSMerkleNode *root, const Key &key)
{
    // Deletes work essentially the same as insertion.
    if(root->IsLeaf()) {
        if(root->hash_ == key)
            return nullptr;
        else
            return root;
    }

    if(root->left_->IsLeaf() && root->left_->hash_ == key)
        return root->right_;
    if(root->right_->IsLeaf() && root->right_->hash_ == key)
        return root->left_;

    KeyDist l_dist = Distance(key, root->left_->hash_),
            r_dist = Distance(key, root->right_->hash_);

    if(l_dist == r_dist)
        return root;
    if(l_dist < r_dist)
        root->left_ = Delete(root->left_, key);
    else
        root->right_ = Delete(root->right_, key);

    return new CSMerkleNode(root->left_, root->right_);
}

CSMerkleNode::operator Json::Value()
{
    Json::Value json_node;
    if(left_ != nullptr)
        json_node["LEFT"] = Json::Value(*left_);
    if(right_ != nullptr)
        json_node["RIGHT"] = Json::Value(*right_);
    json_node["HASH"] = std::string(hash_);
    return json_node;
}

CSMerkleNode *CSMerkleNode::InsertLeaf(CSMerkleNode *leaf, const Key &key)
{
    // Take a leaf node, return a new node (to replace the leaf node),
    // with the leaf node and a new node (constructed from key) as children.
    auto *new_leaf = new CSMerkleNode(key);
    if(key < leaf->hash_)
        return new CSMerkleNode(new_leaf, leaf);
    else if(key > leaf->hash_)
        return new CSMerkleNode(leaf, new_leaf);
    else
        return leaf;
}

bool CSMerkleNode::Contains(CSMerkleNode *root, const Key &key) const
{
    // To see if the three contains a given key, we retrace the steps
    // taken by the insertion algorithm until we hit a leaf. This leaf
    // will be the location of the key if it exists inside the tree.
    if(root->IsLeaf())
        return root->hash_ == key;

    if(root->left_->IsLeaf() && root->left_->hash_ == key)
        return true;
    if(root->right_->IsLeaf() && root->right_->hash_ == key)
        return true;

    KeyDist l_dist = Distance(key, root->left_->hash_),
            r_dist = Distance(key, root->right_->hash_);

    if(l_dist < r_dist)
        return Contains(root->left_, key);
    else if(r_dist < l_dist)
        return Contains(root->right_, key);
    else
        return false;
}

bool CSMerkleNode::IsLeaf() const
{
    // Node w/ no children is leaf.
    return left_ == nullptr && right_ == nullptr;
}

std::string CSMerkleNode::ToString(int level) const
{
    // Primarily for debugging.
    std::string res, tabs(level, '\t');
    res += tabs + "HASH: " + std::string(hash_);
    if(left_)
        res += "\n" + tabs + "LEFT: {\n" + left_->ToString(level + 1) + "\n"
               + tabs + "}";
    if(right_)
        res += "\n" + tabs + "RIGHT: {\n" + right_->ToString(level + 1)
               + "\n" + tabs + "}";

    return res;
}