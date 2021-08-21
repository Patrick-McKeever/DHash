#include "../src/peer.h"
#include <gtest/gtest.h>

/// When new peers join the mesh, will maintenance methods successfully
/// reposition fragments?
TEST(Peer, MaintenanceTest) {

	Peer peer1("127.0.0.1", 5055), peer2("127.0.0.1", 5001),
         peer3("127.0.0.1", 5002), peer4("127.0.0.1", 5003),
         peer5("127.0.0.1", 5004), peer6("127.0.0.1", 5006),
         peer7("127.0.0.1", 5007), peer8("127.0.0.1", 5008),
         peer9("127.0.0.1", 5009), peer10("127.0.0.1", 5010),
         peer11("127.0.0.1", 5011), peer12("127.0.0.1", 5012),
         peer13("127.0.0.1", 5013), peer14("127.0.0.1", 5014),
         peer15("127.0.0.1", 5015), peer16("127.0.0.1", 5016),
         peer17("127.0.0.1", 5017), peer18("127.0.0.1", 5018),
         peer19("127.0.0.1", 5019), peer20("127.0.0.1", 5020),
         peer21("127.0.0.1", 5021), peer22("127.0.0.1", 5022),
         peer23("127.0.0.1", 5023), peer24("127.0.0.1", 5024),
         peer25("127.0.0.1", 5025), peer26("127.0.0.1", 5026),
         peer27("127.0.0.1", 5027), peer28("127.0.0.1", 5028);
	peer1.StartChord();
	peer2.Join("127.0.0.1", 5055);
	peer3.Join("127.0.0.1", 5055);
    peer4.Join("127.0.0.1", 5055);
    peer5.Join("127.0.0.1", 5055);
	peer6.Join("127.0.0.1", 5055);
	peer7.Join("127.0.0.1", 5055);
	peer8.Join("127.0.0.1", 5055);
	peer9.Join("127.0.0.1", 5055);
	peer10.Join("127.0.0.1", 5055);
	peer11.Join("127.0.0.1", 5055);
    peer12.Join("127.0.0.1", 5055);
    peer13.Join("127.0.0.1", 5055);
    peer14.Join("127.0.0.1", 5055);

	EXPECT_TRUE(peer1.Create(Key("1", false), "val"));
	EXPECT_EQ(peer1.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer8.Read(Key("1", false)).Decode(), "val");

	sleep(2);

    peer15.Join("127.0.0.1", 5055);
    peer16.Join("127.0.0.1", 5055);
    peer17.Join("127.0.0.1", 5055);
    peer18.Join("127.0.0.1", 5055);
    peer19.Join("127.0.0.1", 5055);
    peer20.Join("127.0.0.1", 5055);
    peer21.Join("127.0.0.1", 5055);
    peer22.Join("127.0.0.1", 5055);
    peer23.Join("127.0.0.1", 5055);
    peer24.Join("127.0.0.1", 5055);
    peer25.Join("127.0.0.1", 5055);
    peer26.Join("127.0.0.1", 5055);
    peer27.Join("127.0.0.1", 5055);
    peer28.Join("127.0.0.1", 5055);

	// Give time to reposition keys.
	sleep(20);


    EXPECT_EQ(peer1.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer8.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer15.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer21.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer28.Read(Key("1", false)).Decode(), "val");
}

/// Do graceful leaves work properly?
TEST(Peer, LeaveTest) {

    Peer peer1("127.0.0.1", 5055), peer2("127.0.0.1", 5001),
            peer3("127.0.0.1", 5002), peer4("127.0.0.1", 5003),
            peer5("127.0.0.1", 5004), peer6("127.0.0.1", 5006),
            peer7("127.0.0.1", 5007), peer8("127.0.0.1", 5008),
            peer9("127.0.0.1", 5009), peer10("127.0.0.1", 5010),
            peer11("127.0.0.1", 5011), peer12("127.0.0.1", 5012),
            peer13("127.0.0.1", 5013), peer14("127.0.0.1", 5014),
            peer15("127.0.0.1", 5015), peer16("127.0.0.1", 5016),
            peer17("127.0.0.1", 5017), peer18("127.0.0.1", 5018),
            peer19("127.0.0.1", 5019), peer20("127.0.0.1", 5020),
            peer21("127.0.0.1", 5021), peer22("127.0.0.1", 5022),
            peer23("127.0.0.1", 5023), peer24("127.0.0.1", 5024),
            peer25("127.0.0.1", 5025), peer26("127.0.0.1", 5026),
            peer27("127.0.0.1", 5027), peer28("127.0.0.1", 5028);
    peer1.StartChord();
    peer2.Join("127.0.0.1", 5055);
    peer3.Join("127.0.0.1", 5055);
    peer4.Join("127.0.0.1", 5055);
    peer5.Join("127.0.0.1", 5055);
    peer6.Join("127.0.0.1", 5055);
    peer7.Join("127.0.0.1", 5055);
    peer8.Join("127.0.0.1", 5055);
    peer9.Join("127.0.0.1", 5055);
    peer10.Join("127.0.0.1", 5055);
    peer11.Join("127.0.0.1", 5055);
    peer12.Join("127.0.0.1", 5055);
    peer13.Join("127.0.0.1", 5055);
    peer14.Join("127.0.0.1", 5055);

    EXPECT_TRUE(peer1.Create(Key("1", false), "val"));
    EXPECT_EQ(peer1.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer8.Read(Key("1", false)).Decode(), "val");

    sleep(2);

    peer15.Join("127.0.0.1", 5055);
    peer16.Join("127.0.0.1", 5055);
    peer17.Join("127.0.0.1", 5055);
    peer18.Join("127.0.0.1", 5055);
    peer19.Join("127.0.0.1", 5055);
    peer20.Join("127.0.0.1", 5055);
    peer21.Join("127.0.0.1", 5055);
    peer22.Join("127.0.0.1", 5055);
    peer23.Join("127.0.0.1", 5055);
    peer24.Join("127.0.0.1", 5055);
    peer25.Join("127.0.0.1", 5055);
    peer26.Join("127.0.0.1", 5055);
    peer27.Join("127.0.0.1", 5055);
    peer28.Join("127.0.0.1", 5055);

	peer1.Leave();
	peer2.Leave();

    // Give time to reposition keys.
    sleep(20);


    EXPECT_EQ(peer1.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer8.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer15.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer21.Read(Key("1", false)).Decode(), "val");
    EXPECT_EQ(peer28.Read(Key("1", false)).Decode(), "val");
}