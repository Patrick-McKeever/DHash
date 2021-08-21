#include "../src/merkle_node.h"
#include <gtest/gtest.h>

TEST(MerkelNode, Insert) {
	CSMerkleNode root(nullptr, nullptr);
	root.Insert(Key("a", false));
    root.Insert(Key("b", false));
    root.Insert(Key("c", false));

	// Note that we can't verify the element's membership in the merkel tree
	// using "CompactSparseMerkelNode::Contains", because it relies on exactly
	// the same algorithm as "CompactSparseMerkelNode::Insert".
	// Hence, we could encounter conditions in which neither method works,
	// but both register as having worked since they rely on the same
	// methodology.
	// Therefore, it is prudent to typecast to a string instead, since this is a
	// relatively simple algorithm with a low potential for bugs (and
	// effectively no potential for bugs shared with the insert method.
	std::string expected_str = "HASH: 9e28e020e5ff56e0a3825126a423cda5\n"
							   "LEFT: {\n"
							   "\tHASH: 746062a34a6e5d9a8555feeca11003e4\n"
							   "\tLEFT: {\n"
							   "\t\tHASH: 3f10dbe84cbd5e319b1faf0cb9dda9cf\n"
							   "\t}\n"
							   "\tRIGHT: {\n"
							   "\t\tHASH: 4f3f289869e35a0d820ac4e87987dbce\n"
							   "\t}\n"
							   "}\n"
							   "RIGHT: {\n"
							   "\tHASH: b72f1bcde22957e2bb24d01077785f16\n"
							   "}";
	EXPECT_EQ(std::string(root), expected_str);
	root.Destruct();
}


/// Can Merkle Tree delete keys without upsetting its internal structure?
TEST(MerkelNode, Delete) {
    CSMerkleNode root(nullptr, nullptr);
    root.Insert(Key("a", false));
    root.Insert(Key("b", false));
    root.Insert(Key("c", false));

    root.Delete(Key("a", false));
	EXPECT_FALSE(root.Contains(Key("a", false)));
	EXPECT_TRUE(root.Contains(Key("b", false)));
    EXPECT_TRUE(root.Contains(Key("c", false)));
}

TEST(MerkelNode, Contains) {
	CSMerkleNode root(nullptr, nullptr);
    root.Insert(Key("a", false));
    root.Insert(Key("b", false));
    root.Insert(Key("c", false));

	EXPECT_TRUE(root.Contains(Key("a", false)));
	EXPECT_TRUE(root.Contains(Key("b", false)));
	EXPECT_TRUE(root.Contains(Key("c", false)));
	EXPECT_FALSE(root.Contains(Key("d", false)));
}

TEST(MerkelNode, GetPosition) {
	CSMerkleNode root(nullptr, nullptr);
    root.Insert(Key("a", false));
    root.Insert(Key("b", false));
    root.Insert(Key("c", false));

	EXPECT_EQ(std::string(root.GetPosition({ 0, 1 }).hash_),
	          "4f3f289869e35a0d820ac4e87987dbce");
}