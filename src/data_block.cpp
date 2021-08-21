#include "data_block.h"

#include <utility>
#include <cassert>

IDA::IDA(int n, int m, int p)
    : n_(n)
    , m_(m)
    , p_(p)
{}

TwoDimMatrix IDA::Encode(const OneDimMatrix &message) const
{
    auto length = int(message.size());
    TwoDimMatrix a(n_, OneDimMatrix(m_, 0)),
            c(n_, OneDimMatrix(length / m_, 0));

    for(int i = 0; i < n_; i++)
        for(int j = 0; j < m_; j++)
            a[i][j] = pow(1 + i, j);

    for(int i = 0; i < n_; i++)
        for(int j = 0; j < (length / m_); j++)
            for(int k = 0; k < m_; k++)
                c[i][j] += a[i][k] * message[j * m_ + k];

    return c;
}

OneDimMatrix IDA::Decode(const TwoDimMatrix &encoded,
                         const std::vector<int> &fid) const
{
    unsigned long num_rows = encoded.size() * encoded[0].size();
    TwoDimMatrix a(m_, OneDimMatrix(m_));
    OneDimMatrix dm(num_rows);

    for(int i = 0; i < m_; i++)
        for(int j = 0; j < m_; j++)
            a[i][j] = pow(fid[i], j);

    TwoDimMatrix ia = Invert(a);

    for(int i = 0; i < num_rows; i++)
        for(int k = 0; k < m_; k++)
            dm[i] += ia[i % m_][k] * encoded[k][i / m_];

    for(int i = 0; i < dm.size(); i++)
        dm[i] = round(dm[i]);
    return dm;
}

std::string IDA::EncodeAsStr(const std::string &str) const
{
    std::string res;
    StringArr split_by_space = Split(str, " ");
    auto num_words = int(split_by_space.size());
    OneDimMatrix message(num_words, 0);

    for(int i = 0; i < num_words; i++)
        message[i] = std::stod(split_by_space[i]);

    TwoDimMatrix encoded = Encode(message);

    for(int i = 0; i < n_; i++) {
        res += std::to_string(i + 1) + ":";
        for(int j = 0; j < num_words / n_; j++)
            res += std::to_string(encoded[i][j]) + " ";

        res += "\n";
    }

    return res;
}

OneDimMatrix IDA::DecodeFromStr(const std::string &str) const
{
    std::string rme;
    StringArr sms = Split(str, "\n");
    auto sl = int(sms.size());
    StringArr id(sl);

    for(int i = 0; i < sl; i++) {
        StringArr tm = Split(sms[i], ":");
        id[i] = tm[0];
        sms[i] = tm[1];
    }

    StringArr fl = Split(sms[0], " ");
    auto f = int(fl.size());
    std::vector<StringArr> smess(sl, StringArr(f));
    std::vector<int> fid(sl);
    TwoDimMatrix mess(sl, OneDimMatrix(f));

    for(int i = 0; i < sl; i++)
        smess[i] = Split(sms[i], " ");

    for(int i = 0; i < sl; i++)
        fid[i] = stoi(id[i]);

    for(int i = 0; i < sl; i++)
        for(int j = 0; j < f; j++)
            mess[i][j] = std::stod(smess[i][j]);

    return Decode(mess, fid);
}

std::string IDA::DecodeAsStr(const std::string &str) const
{
    std::string rme;
    OneDimMatrix rm = DecodeFromStr(str);
    for(double i : rm)
        rme += std::to_string(int(round(i))) + " ";

    return rme;
}

DataFragment::DataFragment(OneDimMatrix matrix, int index)
                            : fragment_(std::move(matrix))
                            , index_(index)
{}

DataFragment::DataFragment(const std::string& serialized_frag)
{
	printf("SERIALIZED FRAG: %s", serialized_frag.c_str());
    StringArr tm = Split(serialized_frag, ":");
    index_ = stoi(tm[0]);
    for(const auto &frag_el : Split(tm[1], " "))
	    fragment_.push_back(std::stod(frag_el));
}

DataFragment::operator OneDimMatrix() const
{
    return fragment_;
}

DataFragment::operator std::string() const
{
    std::string fragment_str;
    for(double value : fragment_)
        fragment_str += std::to_string(value) + " ";
    // Remove trailing space.
    fragment_str.pop_back();

    return std::to_string(index_) + ":" + fragment_str + "\n";
}

bool operator == (const DataFragment &df1, const DataFragment &df2)
{
    return df1.fragment_ == df2.fragment_ && df1.index_ == df2.index_;
}

bool operator < (const DataFragment &df1, const DataFragment &df2)
{
	return df1.index_ < df2.index_;
}

std::vector<DataFragment> FragsFromMatrix(const TwoDimMatrix &matrix)
{
    std::vector<DataFragment> frags;
    for(int i = 0; i < matrix.size(); i++)
        frags.push_back(DataFragment(matrix[i], i + 1));
    return frags;
}

DataBlock::DataBlock(const std::string &input, bool sanity_check)
                        : ida_(14, 10, 40)
{
	if (input.size() > 40)
		throw std::runtime_error("Cannot encode, input too large");

	for (const char &c : input) {
		auto char_utf = double(c);

		// For some reason, IDA has issues with large values.
		// And I don't know nearly enough linear algebra to debug it.
		if (char_utf < 1000) original_.push_back(double(c));
		else
			throw std::runtime_error(
					std::string("Cannot encode character %c", c));
	}

    // Push an empty UTF code to back of buffer as padding.
    while(original_.size() < 40)
        original_.push_back(0);

    fragments_ = FragsFromMatrix(ida_.Encode(original_));

    // Because of aforementioned issues with IDA class, if a sanity
    // check is requested, we will check to ensure that the encoded frags
    // can be adequately decoded to match the original text.
    if(sanity_check) {
        TwoDimMatrix first_ten_frags(fragments_.cbegin(),
                                     fragments_.cbegin() + 10);
        std::vector<int> indices = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        assert(ida_.Decode(first_ten_frags, indices) == original_);
    }
}

DataBlock::DataBlock(const std::string &encoded_str)
                        : ida_(14, 10, 40)
{
    StringArr split_by_line = Split(encoded_str, "\n");

    if(split_by_line.size() < 10)
        throw std::runtime_error("10 or more fragment are required.");

    // IDA::Decode (which is used in IDA::DecodeFromStr) requires 2d-vector
    // with a number of rows equivalent to the value passed for m_ (in this
    // case 10). Split the string, get the first 10 lines, join then by
    // newline, and pass that to IDA::DecodeFromStr.
    StringArr first_ten_lines(split_by_line.cbegin(),
                              split_by_line.cbegin() + 10);
    std::string truncated;
    for (StringArr::const_iterator i = first_ten_lines.begin();
         i != first_ten_lines.end(); i++)
        truncated += *i + "\n";
    // Remove trailing newline.
    truncated.pop_back();

    original_  = ida_.DecodeFromStr(truncated);
    fragments_ = FragsFromMatrix(ida_.Encode(original_));
}

DataBlock::DataBlock(const std::vector<DataFragment> &fragments)
                        : ida_(14, 10, 40)
{
    // Create fragments and indices.
    std::vector<int> frag_indices;
    TwoDimMatrix frag_matrix;
    for(const DataFragment &fragment : fragments) {
        frag_indices.push_back(fragment.index_);
        frag_matrix.push_back(fragment.fragment_);
    }

    // This may seem redundant. Why decode original and then re-encode it?
    // The answer is because the IDA::Decode method requires only a fraction
    // of the total fragments produced from encoding (in this case, only 10
    // of the 14 fragments produced from encoding are needed to decode.)
    // As a result, we cannot simply pass a list of fragments created from
    // frag_indices and frag_matrix to "FragsFromMatrix", we must instead
    // re-generate all 14 fragments, in case less than 14 were passed to us.
    original_ = ida_.Decode(frag_matrix, frag_indices);
    fragments_ = FragsFromMatrix(ida_.Encode(original_));
}

DataBlock::operator std::string const()
{
    std::string res;
    for(const DataFragment &fragment : fragments_)
        res += std::string(fragment);

    // Remove final "\n".
    res.pop_back();
    return res;
}

std::string DataBlock::Decode() const
{
    std::string res;

    // "original_" is a double vector consisting of UTF codes, possibly
    // with padding.
    for(const double &utf_code : original_) {
        // 0 codes are used to pad end of buffer
        if(utf_code == 0)
            break;

        res += char(utf_code);
    }
    return res;
}

bool operator == (const DataBlock &db1, const DataBlock &db2)
{
    return db1.original_ == db2.original_ && db1.fragments_ == db2.fragments_;
}