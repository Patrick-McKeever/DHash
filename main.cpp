#include <iostream>
#include <gtest/gtest.h>
#include <json/json.h>
#include "src/peer.h"
#include "src/finger_table.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}