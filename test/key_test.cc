#include <gtest/gtest.h>
#include "../src/key.h"

TEST(KeyInBetweenTest, ExclusiveNoModulo) {
    Key key1(75), key2(99);
    EXPECT_TRUE(key1.InBetween(0, 99, false));
    EXPECT_FALSE(key2.InBetween(0, 99, false));
}

TEST(KeyInBetweenTest, ExclusiveWithModulo) {
    Key key1(1), key2(25);
    EXPECT_TRUE(key1.InBetween(75, 25, false));
    EXPECT_FALSE(key2.InBetween(75, 25, false));
}

TEST(KeyInBetweenTest, InclusiveNoModulo) {
    Key key1(75), key2(99);
    EXPECT_TRUE(key1.InBetween(0, 99, true));
    EXPECT_TRUE(key2.InBetween(0, 99, true));
}

TEST(KeyInBetweenTest, InclusiveWithModulo) {
    Key key1(1), key2(25);
    EXPECT_TRUE(key1.InBetween(75, 25, true));
    EXPECT_TRUE(key2.InBetween(75, 25, true));
}

TEST(KeyInBetweenTest, DifferingLengths) {
	// This was previously an edge case. The differing lengths of the keys
	// produced an inaccurate value for hex codes, so now we simply assume
	// a constant keyspace of 16^32 keys.
	Key key("f4ee136cb4059b2883450e7e93698be", true),
        lb("633bd46b5c515992a5ce553d0680bec9", true),
        ub("f4ee136cb4059b2883450e7e93698bd", true);

	EXPECT_FALSE(key.InBetween(lb, ub, true));
}