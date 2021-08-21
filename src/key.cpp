#include "key.h"
#include <utility>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sstream>

namespace mp = boost::multiprecision;

/**
 * Generate a SHA-1 hash (stored as boost::uuids::uuid) from plaintext.
 *
 * @param plaintext Plaintext to hash.
 * @return Sha1 hash of plaintext.
 */
boost::uuids::uuid GenerateSha1Hash(const std::string &plaintext)
{
    boost::uuids::name_generator_sha1 generator(boost::uuids::ns::dns());
    return generator(plaintext);
}

/**
 * Convert integer to hex string.
 *
 * @param val Some sort of integer value.
 * @return Int as hexadecimal stirng.
 */
template <typename int_type>
std::string IntToHexStr(int_type val)
{
    std::stringstream hex_stream;
    hex_stream << std::hex << val;
    return hex_stream.str();
}

Key::Key(const std::string &key,
         bool hashed)
    : plaintext_(hashed ? "" : key)
{
    if(hashed) {
        // Boost interprets numeric strings beginning with 0x as hashes.
        value_ = boost::multiprecision::uint256_t("0x" + key);
    } else {
        boost::uuids::uuid uuid = GenerateSha1Hash(key);
        value_ = boost::multiprecision::uint256_t (uuid);
    }

    string_ = IntToHexStr(value_);
}

Key::Key(mp::uint256_t key) :
        value_(std::move(key)),
        string_(IntToHexStr(value_)) {}

unsigned long long Key::Size() const
{
    return IntToHexStr(value_).length();
}

bool Key::InBetween(const mp::uint256_t &lower_bound,
                    const mp::uint256_t &upper_bound,
                    bool inclusive) const
{
    // If upper and lower bound are same value, see if value is equal to either.
    if (lower_bound == upper_bound) {
        if (value_ == upper_bound) return true;
        else return false;
    }

    // Modulo the upper bound, lower bound, and value by number keys in ring.
    mp::cpp_int keys_in_ring = mp::pow(mp::cpp_int(16), 32);
    mp::cpp_int mod_lower_bound = lower_bound % keys_in_ring;
    mp::cpp_int mod_upper_bound = upper_bound % keys_in_ring;
    mp::cpp_int mod_value = value_ % keys_in_ring;

    // Now compare.
    if (lower_bound < upper_bound) {
        return (inclusive ?
                 mod_lower_bound <= mod_value && mod_value <= upper_bound :
                 mod_lower_bound < mod_value && mod_value < upper_bound);
    } else {
        // if in [b, a] then not in [a, b]
        return !(inclusive ?
                  mod_upper_bound < mod_value && mod_value < mod_lower_bound :
                  mod_upper_bound <= mod_value && mod_value <= mod_lower_bound);
    }
}

bool Key::InBetween( const mp::uint256_t &lower_bound,
                     const mp::uint256_t &upper_bound,
                     bool inclusive)
{
    // Cast away the const from the non-const version of this method.
    return (const_cast<const Key*>(this)->InBetween(lower_bound, upper_bound,
	                                                 inclusive));
}

Key::operator boost::multiprecision::uint256_t() const  {   return value_;                                      }
Key::operator boost::multiprecision::cpp_int()   const  {   return boost::multiprecision::cpp_int(value_);      }
Key::operator std::string()                      const  {   return string_;                                     }

bool operator == (const Key &key1, const Key &key2)     {   return key1.value_ == key2.value_;                  }
bool operator != (const Key &key1, const Key &key2)     {   return key1.value_ != key2.value_;                  }
bool operator <  (const Key &key1, const Key &key2)     {   return key1.value_ < key2.value_;                   }
bool operator >  (const Key &key1, const Key &key2)     {   return key1.value_ > key2.value_;                   }
bool operator <= (const Key &key1, const Key &key2)     {   return key1.value_ <= key2.value_;                  }
bool operator >= (const Key &key1, const Key &key2)     {   return key1.value_ >= key2.value_;                  }

Key operator + (const Key &key, int number)             {   return Key(key.value_ + mp::uint256_t (number));    }
Key operator - (const Key &key, int number)             {   return Key(key.value_ - mp::uint256_t(number));     }
Key operator + (const Key &key1, const Key &key2)       {   return Key(key1.value_ + key2.value_);              }
Key operator - (const Key &key1, const Key &key2)       {   return Key(key1.value_ - key2.value_);              }