/**
 * data_block.h
 *
 * This file aims to implement the necessary steps for information dispersal.
 * Though it contains 3 classes (all implemented in the '.h' file in anticip-
 * ation of the likely conversion of DataBlock into a template class at some
 * point in the future), I have opted to place them all within the same file,
 * since the IDA and DataFragment classes are only useful insofar as they
 * support the implementation of the DataBlock class.
 *
 * This file should accomplish the following tasks:
 *      - Implement a class IDA which supports the encoding of 1D matrices
 *        as 2D matrices; the IDA class should subsequently be able to
 *        decode the original matrix from a fraction of the rows in the 2D
 *        matrix, based on principles outlined in Michael Rabin's 'The
 *        Information Dispersal Algorithm and its Applications'
 *        (https://sci-hub.se/10.1007/978-1-4612-3352-7_32).
 *      - Implement a class DataFragment which represents a single row of
 *        a matrix resultant from IDA encoding, storing its index and its
 *        data.
 *      - Implement a class DataBlock which can decode from a serialized string,
 *        a list of data fragments, or encode a string.
 *
 * These classes will form the basis of the replication strategy outlined for
 * the DHash algorithm (see Josh Cates' thesis: 
 * https://pdos.csail.mit.edu/papers/chord:cates-meng.pdf). In DHash,
 * each of a peer's n successors stores a distinct fragment of the encoded data,
 * allowing for replication of the data without requiring a full replica on
 * multiple different peers.
 */

#pragma once
#ifndef CHORD_FINAL_DATA_BLOCK_H
#define CHORD_FINAL_DATA_BLOCK_H

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

/// Turns out IDAs are linear-algebra heavy.
/// Just gonna make my life a little easier here.
typedef std::vector<double> OneDimMatrix;
typedef std::vector<OneDimMatrix> TwoDimMatrix;

/// Vector of strings.
typedef std::vector<std::string> StringArr;

/**
 * DHash proposes the use of an Information Dispersal Algorithm (IDA),
 * which segments a block of data with length L into n pieces, each with
 * size L/m. In this scenario, only m blocks are necessary to reconstruct
 * the original data block.
 *
 * I understand very little of how this works. This code is primarily based on
 * the description offered in Michael Rabin's 'The Information Dispersal
 * Algorithm and its Applications' (https://sci-hub.se/10.1007/978-1-4612-3352-7_32)
 * and a Java implementation on github(https://github.com/rkj2096/IDA),
 * so, while I was able to create a functional implementation, I was not able to
 * document it particularly well due to a lack of knowledge about the underlying
 * linear algebra.
 */
class IDA {
public:
	/**
	 * Constructor.
	 *
	 * @param n Total number fragments produced per block.
	 * @param m Minimum number replicas necessary to reconstruct a block.
	 * @param p Prime number used for...something?
	 */
    IDA(int n, int m, int p);

	/**
	 * Encode an array of doubles as a 2D matrix, with each row as a new data
	 * fragment. (i.e. with m fragments, you can reconstruct the original arr.)
	 *
	 * @param message Array of doubles to encode
	 * @return Array of encoded data fragments.
	 */
    [[nodiscard]] TwoDimMatrix Encode(const OneDimMatrix &message) const;

	/**
	 * Decode a list of encoded fragments into a data block given fragments
	 * and their indices.
	 *
	 * @param encoded A vector containing (at least m_) encoded fragments of
	 *                the original matrix, with each fragment represented as
	 *                an array of doubles.
	 * @param fid A vector specifying the index of the fragments in 'encoded',
	 *            such that the index of the nth entry in 'encoded' is given
	 *            by the nth entry of 'fid'.
	 * @return The decoded one-d matrix (array of doubles).
	 */
    [[nodiscard]] OneDimMatrix Decode(const TwoDimMatrix &encoded,
                                      const std::vector<int> &fid) const;

	/**
	 * Take a string of space-delimited numbers, parse as array of numbers,
	 * return data fragments of encoded array as string of form.:
	 *      [FRAG_1]:[num_1] [num_2]...[num_(p_/m_)]
	 *      [FRAG_2]:[num_1] [num_2]...[num_(p_/m_)]
	 *      ...
	 *      [FRAG_(n_)]:[num_1] [num_2]...[num_(p_/m_)]
	 *
	 * @param str String of space delimited numbers to encode as a string
	 *            newline-delimited fragments consisting of space-delimited
	 *            numbers.
	 * @return Encoded string in form described above.
	 */
    [[nodiscard]] std::string EncodeAsStr(const std::string &str) const;

    /**
     * Produce a 1D double vector from string encoded via "IDA::EncodeAsStr".
     * It is useful to define encode and decode operations to operate
     * specifically on strings, because we will frequently pass data from peer
     * to peer. Since there are cases where an intermediate peer may seek
     * to decode the data only to forward to another peer, it would be redundant
     * to decode into a OneDimMatrix only to subsequently re-serialize that
     * matrix as a string.
     *
     * @param str String encoded in form:
     *      [FRAG_1]:[num_1] [num_2]...[num_(p_/m_)]
     *      [FRAG_2]:[num_1] [num_2]...[num_(p_/m_)]
     *      ...
     *      [FRAG_(n_)]:[num_1] [num_2]...[num_(p_/m_)]
     * @return Decoded array represented of doubles.
     */
	[[nodiscard]] OneDimMatrix DecodeFromStr(const std::string &str) const;

	/**
	 * Decode string representing encoded info into space-delimited string
	 * of doubles.
	 *
	 * @param str String encoded in form:
     *      [FRAG_1]:[num_1] [num_2]...[num_(p_/m_)]
     *      [FRAG_2]:[num_1] [num_2]...[num_(p_/m_)]
     *      ...
     *      [FRAG_(n_)]:[num_1] [num_2]...[num_(p_/m_)]
	 * @return Space-delimited string of doubles.
	 */
    [[nodiscard]] std::string DecodeAsStr(const std::string &str) const;

	/// Parameters pertaining to IDA encoding. n=14, m=10, p=40 is ideal.
	/// Possibly mark these static in the future.
    int n_, m_, p_;
};

/**
 * The IDA will produce a 2D matrix of doubles. This matrix can be reconstructed
 * in full from only a handful of the rows in that matrix, so long as the index
 * of those rows in the matrix are known.
 * This data structure exists to represent a single row. It should:
 *      - Hold the vector of doubles corresponding to a single row.
 *      - Hold the index of said row.
 *      - Be able to be serialized into a string.
 */
class DataFragment {
public:
	/**
	 * Construct from vector of doubles and index.
	 *
	 * @param matrix One row of matrix from IDA::Encode.
	 * @param index Index of row in said matrix.
	 */
	DataFragment(OneDimMatrix matrix, int index);

	/**
	 * Construct fragment from serialized string.
	 *
	 * @param serialized_frag Output from string operator.
	 */
	DataFragment(const std::string &serialized_frag);

	/**
	 * Convert to a vector of doubles (i.e. return fragment_).
	 *
	 * @return fragment_
	 */
	explicit operator OneDimMatrix() const;

	/**
	 * Serialize fragment.
	 *
	 * @return string of form "[INDEX]:[VAL_1] [VAL_2]...[VAL_N]\n"
	 */
	operator std::string() const;

	/**
	 * Comparison operator.
	 *
	 * @param df1 Lefthand.
	 * @param df2 Righthand.
	 * @return Equality of left and right hands.
	 */
    friend bool operator == (const DataFragment &df1, const DataFragment &df2);

	/**
	 * Less than operator (for implementation of sets/maps of DataFragments).
	 * @param df1 Lefthand.
	 * @param df2 Righthand.
	 * @return Is df1's index less than df2's?
	 *         (Note: this is useless for practical comparison purposes,
	 *          but it allows for insertion into sets and maps.)
	 */
	friend bool operator < (const DataFragment &df1, const DataFragment &df2);

    /// A vector of doubles representing a row from a matrix given by
    /// IDA::Encode.
    OneDimMatrix fragment_;

	/// Index of the fragment. (e.g. IDA produced 14 fragments, this is nth).
    int index_;
};

/**
 * Convert a 2-dimensional matrix of encoded data (result of an IDA::encode
 * call) into a vector of fragments.
 *
 * @param matrix Encoded data.
 * @return List of fragments initialized from matrix.
 */
std::vector<DataFragment> FragsFromMatrix(const TwoDimMatrix &matrix);

/**
 * The DataBlock class represents a piece of data corresponding to a key.
 * Its implementation should allow the ability to:
 *      - Encode into fragments using an information dispersal algorithm.
 *      - Decode from only a fraction of the total fragments generated for
 *        any given block (e.g. 14 fragments generated, only 10 needed to
 *        decode.
 *      - Serialize as a string.
 *      - Construct from a serialized string (e.g. parse the string).
 *      - Construct from a vector of DataFragments.
 * This will enable replication of data on multiple peers.
 */
class DataBlock {
public:
	/**
	 * Constructor #1.
	 * Create a data block by first encoding an input string as a vector of
	 * doubles corresponding to its characters' UTF codes, then encode
	 * that vector as data fragments.
	 *
	 * @param input String to encode.
	 * @param sanity_check Should the constructor check whether encoding
	 *                     and decoding is possible with the given input?
	 */
	DataBlock(const std::string &input, bool sanity_check);

	/**
	 * Constructor #2.
	 * Decode from an encoded string to create a data block.
	 *
	 * @param encoded_str String from which we will decode.
	 */
	explicit DataBlock(const std::string &encoded_str);

	/**
	 * Constructor #3.
	 * Decode from an array of data fragments.
	 *
	 * @param fragments An array of data fragments.
	 */
	explicit DataBlock(const std::vector<DataFragment> &fragments);

	/**
	 * Convert data block into string.
	 * (Will there be issues that we're giving constructor 2 14 els instead of 10?
	 * @return Data block serialzied in form:
	 *      [FRAG_1]:[num_1] [num_2]...[num_(p_/m_)]
     *      [FRAG_2]:[num_1] [num_2]...[num_(p_/m_)]
     *      ...
     *      [FRAG_(n_)]:[num_1] [num_2]...[num_(p_/m_)]
	 */
	operator std::string const();

    /**
     * Convert DataBlock to string by decoding "original_".
     *
     * @return The string used to create this DataBlock - i.e. the argument
     *         passed as "input" to the first constructor.
     */
	[[nodiscard]] std::string Decode() const;

	/**
	 * Comparison operator (for unit tests).
	 *
	 * @param db1 Lefthand.
	 * @param db2 Righthand.
	 * @return Equality of left and right hand.
	 */
	friend bool operator == (const DataBlock &db1, const DataBlock &db2);

	/// Used to encode, decode, serialize, and parse matrices.
	IDA ida_;

	/// The original string translated into a vector of doubles, with
	/// the nth double corresponding to string's nth char's UTF code.
	OneDimMatrix original_;

	/// A two-d vector containing one-d vectors of doubles, with each one-d
	/// double vector representing a "fragment". These can be decoded into
	/// a single one-d double vector ("original_"), which can in turn be
	/// decoded into a string.
	std::vector<DataFragment> fragments_;
};

/**
 * Partial Pivot Gaussian Elimination. No idea how this works.
 *
 * @param matrix Matrix which will be pivoted in-place.
 * @param index Another vector to be modified in-place. Idk.
 */
static void PartialPivotGaussElim(TwoDimMatrix &matrix, std::vector<int> &index)
{
    unsigned long index_size = index.size();
    OneDimMatrix c(index_size, 0);

    for(int i = 0; i < index_size; i++)
        index[i] = i;

    for(int i = 0; i < index_size; i++) {
        double c1 = 0;
        for(int j = 0; j < index_size; j++) {
            double c0 = abs(matrix[i][j]);
            if(c0 > c1) c1 = c0;
        }
        c[i] = c1;
    }

    int k = 0;
    for(int j = 0; j < index_size - 1; j++) {
        double pi1 = 0;
        for(int i = j; i < index_size; i++) {
            double pi0 = abs(matrix[index[i]][j]);
            pi0 /= c[index[i]];
            if(pi0 > pi1) {
                pi1 = pi0;
                k = i;
            }
        }

        int itmp = index[j];
        index[j] = index[k];
        index[k] = itmp;

        for(int i = j + 1; i < index_size; i++) {
            double pj = matrix[index[i]][j] / matrix[index[j]][j];
            matrix[index[i]][j] = pj;
            for(int l = j + 1; l < index_size; l++)
                matrix[index[i]][l] -= pj * matrix[index[j]][l];
        }
    }
}
/**
 * Invert a matrix - i.e. find the matrix which, when multiplied by the input
 * matrix, will produce the identity matrix.
 *
 * @param matrix Matrix to invert.
 * @return Inverted version of matrix.
 */
static TwoDimMatrix Invert(TwoDimMatrix &matrix)
{
    int num_rows = int(matrix.size());
    TwoDimMatrix x(num_rows, OneDimMatrix(num_rows, 0)),
            b(num_rows, OneDimMatrix(num_rows, 0));
    std::vector<int> index(num_rows, 0);

    for(int i = 0; i < num_rows; i++)
        b[i][i] = 1;

    PartialPivotGaussElim(matrix, index);

    for(int i = 0; i < num_rows - 1; i++)
        for(int j = i + 1; j < num_rows; j++)
            for(int k = 0; k < num_rows; k++)
                b[index[j]][k] -= matrix[index[j]][i] * b[index[i]][k];

    // Alter column-by-column.
    for(int i = 0; i < num_rows; i++) {
        x[num_rows - 1][i] = (b[index[num_rows - 1]][i] /
                              matrix[index[num_rows - 1]][num_rows - 1]);
        for(int j = num_rows - 2; j >= 0; j--) {
            x[j][i] = b[index[j]][i];
            for(int k = j + 1; k < num_rows; k++) {
                x[j][i] -= (matrix[index[j]][k] * x[k][i]);
            }
            x[j][i] /= matrix[index[j]][j];
        }
    }

    return x;
}

/**
 * Split a string into a vector of substrings based on delimiter.
 *
 * @param s String to split.
 * @param delimiter The substring which delimits the substrings in the result.
 * @return A vector of substrings.
 */
static StringArr Split(const std::string &s, const std::string &delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    StringArr res;

    while ((pos_end = s.find (delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}

#endif
