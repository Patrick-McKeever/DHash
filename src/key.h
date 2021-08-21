/**
 * key.h
 *
 * The Key class exists as a wrapper for values on a circular node. Its primary
 * reason for existing is to implement a clockwise-between method
 * (Key::InBetween), which will allow for peers to determine the location of
 * keys in relation to other keys in a chord ring through a bit of modular
 * arithmetic.
 */
#ifndef CHORD_FINAL_KEY_H
#define CHORD_FINAL_KEY_H

#include <boost/uuid/uuid.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <string>

class Key {
public:
    /**
     * Constructor 1: Generate a key from a string.
     *
     * @param key Either a numeric string or an unhashed string.
     * @param hashed Is string already-hashed (i.e. numeric),
     *               or do we need to hash it?
     */
    Key(const std::string &key, bool hashed);

    /**
     * Constructor 2: Generate a key from a boost::multiprecision::uint128_t.
     *
     * @param key A numeric value.
     */

    Key(boost::multiprecision::uint256_t key);

    /**
     * Is key "in between" lower_bound and upper_bound on a logical ring?
     *
     * @param lower_bound Lower bound of query.
     * @param upper_bound Upper bound of query.
     * @param inclusive Is range inclusive?
     * @return Whether or not this key is within specified range on logical ring.
     */
    bool InBetween(const boost::multiprecision::uint256_t &lower_bound,
                          const boost::multiprecision::uint256_t &upper_bound,
                          bool inclusive) const;

    /**
     * Return key size.
     *
     * @return this->key_size_
     */
    unsigned long long Size() const;

    /**
     * Non-const version of above function.
     *
     * @param lower_bound Lower bound of query.
     * @param upper_bound Upper bound of query.
     * @param inclusive Is range inclusive?
     * @return Whether or not this key is within specified range on logical ring.
     */
    bool InBetween(const boost::multiprecision::uint256_t &lower_bound,
                    const boost::multiprecision::uint256_t &upper_bound,
                    bool inclusive);

    /**
     * Overload typecast to boost:multiprecision::uint256_t for const instances of Key.
     * @return this->value_
     */
    operator boost::multiprecision::uint256_t() const;

    /**
     * Overload typecast to boost::multiprecision::cpp_int.
     *
     * @return Key->value_ as cpp_int.
     */
    operator boost::multiprecision::cpp_int() const;

    /**
     * Overload typecast to std::string.
     *
     * @return Key::string_ (a string version of Key::value_).
     */
    operator std::string() const;

    /// Overload operators for numeric comparison using Key::value_.
    friend bool operator ==     (const Key &key1, const Key &key2);
    friend bool operator !=     (const Key &key1, const Key &key2);
    friend bool operator <      (const Key &key1, const Key &key2);
    friend bool operator >      (const Key &key1, const Key &key2);
    friend bool operator <=     (const Key &key1, const Key &key2);
    friend bool operator >=     (const Key &key1, const Key &key2);

    /**
     * Add a key to some numerical type.
     *
     * @param key The lefthand equation side, a key whose value will be incremented by "number".
     * @param number The number by which "key"'s value will be incremented.
     * @return A new key, identical to this key, except that the value will be increased by "number".
     */
    friend Key operator +       (const Key &key, int number);
	friend Key operator -       (const Key &key, int number);
	friend Key operator +       (const Key &key1, const Key &key2);
	friend Key operator -       (const Key &key1, const Key &key2);

private:
    /// Numeric value of key stored as boost uuid.
    boost::multiprecision::uint256_t value_;

    /// String representation of key.
    std::string string_;

    /// Plaintext of the hashed key, if given to us in a constructor.
    /// If plaintext is unknown, Key::plaintext_ is left empty.
    /// Useful primarily for debugging.
    std::string plaintext_;
};

#endif
