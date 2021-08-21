#include "finger_table.h"
#include <iomanip>

FingerTable::FingerTable(Key starting_key)
                            : starting_key_(std::move(starting_key))
                            // Num keys in chord is 16 ^ hex-id-length.
                            , keys_in_chord_(mp::pow(mp::cpp_int(16),
                                                     starting_key.Size()))
                            // Num entries is binary ID length of key.
                            , num_entries_(4 * 32)
{}

void FingerTable::AddFinger(const Finger &finger)
{
    table_.push_back(finger);
}

Finger FingerTable::GetNthEntry(int n)
{
    return table_.at(n);
}

PeerRepr FingerTable::Lookup(const Key &key)
{
    for (const Finger &finger: table_) {
        bool key_in_range = key.InBetween(finger.lower_bound_,
		                                  finger.upper_bound_,
                                          true);
        if(key_in_range)
            return finger.successor_;
    }

    throw std::runtime_error("Key not found");
}

void FingerTable::EditNthFinger(int n, const PeerRepr &succ)
{
    table_.at(n).successor_ = succ;
}

void FingerTable::AdjustFingers(const PeerRepr &new_peer)
{
    for(auto &finger : table_)
        if(finger.lower_bound_.InBetween(new_peer.min_key_, new_peer.max_key_,
                                         true))
            finger.successor_ = new_peer;
}

std::pair<Key, Key> FingerTable::GetNthRange(int n)
{
    mp::cpp_int starting_key = starting_key_;
    mp::cpp_int lb_increment_value = mp::pow(mp::cpp_int(2), n);
    auto lower_bound = mp::uint256_t((starting_key + lb_increment_value)
                                     % keys_in_chord_);
    mp::cpp_int ub_increment_value = mp::pow(mp::cpp_int(2), n + 1);
    auto upper_bound = mp::uint256_t((starting_key + ub_increment_value)
                                     % keys_in_chord_) - 1;
    return { Key(lower_bound), Key(upper_bound) };
}

// This method will pay dividends during debugging.
FingerTable::operator std::string()
{
    // Since ranges start out so small, we need to visually condense this info.
    // To do so, we collate ranges of keys that are succeeded by the same peer.
	std::vector<Finger> display_fingers;
	for(const auto &finger : table_) {
		if(display_fingers.empty())
			display_fingers.push_back(finger);
		else if(display_fingers.back().successor_ == finger.successor_)
			display_fingers.back().upper_bound_ = finger.upper_bound_;
		// If this seems redundant, bear in mind that trying to get the
		// successor of back if it's empty would cause a segfault.
		else
			display_fingers.push_back(finger);
    }

	std::stringstream res;
	// Create upper border, column names.
	res << std::string(131, '-') << "\n";
    res << std::setfill(' ') <<
        "| " << "LOWER BOUND" <<
        std::setw(26) << "| " << "UPPER BOUND" <<
        std::setw(26) << "| " << "SUCC ID" <<
        std::setw(30) << "| " << "SUCC IP:PORT" <<
        std::setw(7) << "|\n";

    res << std::string(131, '-') << "\n";
	// Max key size (and also the normal size is 32 digits hex. We set the width
	// to 5 for 32 bit keys, so, to ensure alignment, we need to make sure that
	// the width is one char larger for every digit that is missing in the key
	// for any given column.
	for(const auto &finger : display_fingers)
		res << std::setfill(' ') << "| " << std::setw(5)
			<< std::string(finger.lower_bound_)
            << std::setw(5 + (32 - finger.lower_bound_.Size())) << "| "
			<< std::string(finger.upper_bound_)
			<< std::setw(5 + (32 - finger.upper_bound_.Size())) << "| "
			<< std::string(finger.successor_.id_)
			<< std::setw(5 + (32 - finger.successor_.id_.Size())) << "| "
			<< std::string(finger.successor_.ip_addr_) << ":"
			<< std::to_string(finger.successor_.port_) << std::setw(5) << "|\n";
    res << std::string(131, '-') << "\n";
    return res.str();
}

bool FingerTable::Empty() {
	return table_.empty();
}