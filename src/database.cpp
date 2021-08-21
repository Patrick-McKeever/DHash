#include "database.h"

Database::Database()
    : index_(nullptr, nullptr)
{}

Database::~Database()
{
    // We had to define manual constructor for merkel tree.
    index_.Destruct();
}

void Database::Insert(const std::pair<Key, DataFragment> &key_frag_pair)
{
	if(data_.find(key_frag_pair.first) != data_.end())
        throw std::runtime_error("Key already exists in db");

    index_.Insert(key_frag_pair.first);
    data_.insert(key_frag_pair);
}

DataFragment Database::Lookup(const Key &key)
{
    // Searching merkel tree is quicker than calling map::find.
    if(index_.Contains(key))
        return data_.at(key);
    else
        throw std::runtime_error("Key does not exist in database.");
}

void Database::Update(const std::pair<Key, DataFragment> &key_frag_pair)
{
    std::map<Key, DataFragment>::iterator it;
    if((it = data_.find(key_frag_pair.first)) == data_.end())
        throw std::runtime_error("Key does not exist in database.");
    else
        it->second = key_frag_pair.second;
}

void Database::Delete(const Key &key)
{
    if(index_.Contains(key))
        data_.erase(key);
    else
        throw std::runtime_error("Key does not exist in database.");
}

KeyFragPair *Database::Next(const Key &key)
{
	if(data_.empty())
		return nullptr;

    for(auto &key_str_pair : data_)
        if(key_str_pair.first > key)
            return reinterpret_cast<KeyFragPair*>(&key_str_pair);

    static KeyFragPair ret = *data_.begin();
    return &ret;
}

KeyFragMap Database::ReadRange(const Key &lower_bound, const Key &upper_bound)
{
    std::map<Key, DataFragment> keys_in_range;
    for(auto &[key, frag] : data_)
        if(key.InBetween(lower_bound, upper_bound, true))
            keys_in_range.insert({ key, frag });
    return keys_in_range;
}

bool Database::Contains(const Key &key)
{
    return index_.Contains(key);
}