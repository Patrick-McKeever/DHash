#include "../src/client.h"
#include "../src/server.h"
#include <gtest/gtest.h>
#include <iostream>
#include <json/json.h>
#include <memory>
#include <chrono>
#include <deque>

using namespace std::chrono_literals;

/// NOTE: This class exists exclusively for unit testing.
class RequestClass {
public:
    /**
     * Initialize class with value n to add sub from input values.
     *
     * @param n Value to add/sub from input values.
     */
    explicit RequestClass(int n) : n_(n) {}

    /// Value to add/sub from
    int n_;

    /**
     * Add n to value in JSON request.
     *
     * @param request JSON request with field "value".
     * @return JSON response containing field "value" = [original_value] + n.
     */
    [[nodiscard]] Json::Value add_n(const Json::Value &request) const
    {
        Json::Value resp;

        // If value is present in request, return value + 1, else return error.
        if (request.get("VALUE", NULL) != NULL) {
            resp["VALUE"] = request["VALUE"].asInt() + this->n_;
        } else
			throw std::runtime_error("Invalid value.");

        return resp;
    }

    /**
     * Sun n from value in JSON request.
     *
     * @param request JSON request with field "value".
     * @return JSON response containing field "value" = [original_value] - n.
     */
    [[nodiscard]] Json::Value sub_n(const Json::Value &request) const
    {
        Json::Value resp, value;

        // If value is present in request, return value + 1, else return error.
        if (request.get("VALUE", NULL) != NULL) {
            resp["VALUE"] = request["VALUE"].asInt() - this->n_;
        } else
			throw std::runtime_error("Invalid value.");
        return resp;
    }
};

typedef std::function<Json::Value(RequestClass, const Json::Value &)> RequestClassMethod;

/// Test set up: declare and run server with "ADD_1" and "SUB_1" commands on
/// 127.0.0.1:5000.
class RequestTest : public testing::Test {
public:
    static void SetUpTestSuite() {
        auto *request_inst = new RequestClass(1);
        std::map<std::string, RequestClassMethod> commands {
				{"ADD_1", std::mem_fn(&RequestClass::add_n)},
                {"SUB_1", std::mem_fn(&RequestClass::sub_n)}
		};
        server_ = new Server<RequestClassMethod, RequestClass>(5000, commands,
		                                                       request_inst);
        server_->RunInBackground();

        request_maker_ = new Client;
        // Since the server is running in a new thread, spin until it completes setup.
        std::this_thread::sleep_for(10ms);
    }

    static void TearDownTestSuite() {
        server_->Kill();
    }

    static Server<RequestClassMethod, RequestClass> *server_;
    static Client *request_maker_;
};

Server<RequestClassMethod, RequestClass> *RequestTest::server_ = nullptr;
Client *RequestTest::request_maker_ = nullptr;

/// Does server provide correct responses to valid requests?
TEST_F(RequestTest, ValidRequest) {
    // Formulate request asking for value of 1 + 1.
    Json::Value add_one_req;
    add_one_req["COMMAND"] = "ADD_1";
    add_one_req["VALUE"] = 1;

    // Send request, expect value of 2 and successful return code.
    Json::Value add_one_resp = request_maker_->MakeRequest("127.0.0.1", 5000,
	                                                       add_one_req);
    EXPECT_EQ(2, add_one_resp["VALUE"].asInt());
    EXPECT_TRUE(add_one_resp["SUCCESS"].asBool());

    // Formulate request asking for value of 1 - 1.
    Json::Value sub_one_req;
    sub_one_req["COMMAND"] = "SUB_1";
    sub_one_req["VALUE"] = 1;

    // Send request, expect value of 0 and successful return code.
    Json::Value sub_one_resp = request_maker_->MakeRequest("127.0.0.1", 5000,
	                                                       sub_one_req);
    EXPECT_EQ(0, sub_one_resp["VALUE"].asInt());
    EXPECT_TRUE(add_one_resp["SUCCESS"].asBool());
}

using TestServer = Server<RequestClassMethod, RequestClass>;

/// In this test case, we send a request with an invalid command type.
/// The server should return a failure code with an error outlining as much.
TEST_F(RequestTest, InvalidCommand) {
    Json::Value invalid_req;
    invalid_req["COMMAND"] = "INVALID_COMMAND";

    Json::Value invalid_resp = request_maker_->MakeRequest("127.0.0.1", 5000,
	                                                       invalid_req);
    EXPECT_FALSE(invalid_resp["SUCCESS"].asBool());
    EXPECT_EQ("Invalid command.", invalid_resp["ERRORS"].asString());
}

/// In this test case, we send a request with a valid command but invalid value.
/// The server should return a failure code with a JSON parse error.
TEST_F(RequestTest, InvalidValue) {
    Json::Value invalid_val_req;
    invalid_val_req["COMMAND"] = "ADD_1";
    // JSON parser will be unable to interpret request["VALUE"] as a str.
    invalid_val_req["VALUE"] = "INVALID_VALUE";

    Json::Value invalid_resp = request_maker_->MakeRequest("127.0.0.1", 5000,
	                                                       invalid_val_req);
    EXPECT_FALSE(invalid_resp["SUCCESS"].asBool());
    EXPECT_EQ("Value is not convertible to Int.", invalid_resp["ERRORS"].asString());
}

/// In this test case, we send a request with a valid command but without the required args.
/// The server should return a failure code with a KeyError.
TEST_F(RequestTest, MissingValue) {
    // Formulate request asking for value of 1 + 1.
    Json::Value missing_val_req;
    missing_val_req["COMMAND"] = "ADD_1";

    Json::Value missing_val_resp = request_maker_->MakeRequest("127.0.0.1", 5000,
	                                                           missing_val_req);
    EXPECT_FALSE(missing_val_resp["SUCCESS"].asBool());
    EXPECT_EQ("Invalid value.", missing_val_resp["ERRORS"].asString());
}

/// The server should call handlers (methods of a template parameter
/// "RequestClass") on an up-to-date version of "RequestClass".
/// If we change the members of RequestClass, does the server's behavior
/// change accordingly (i.e. is server able to maintain up-to-date version)?
TEST(ServerMiscellaneous, ModifiedRequestClass) {
    auto *request_inst = new RequestClass(1);
    std::map<std::string, RequestClassMethod> commands {
            {"ADD_1", std::mem_fn(&RequestClass::add_n)},
            {"SUB_1", std::mem_fn(&RequestClass::sub_n)}
    };
    auto *server = new TestServer(5000, commands, request_inst);
	server->RunInBackground();
	Client client;

    // Formulate request asking for value of 1 - 1.
    Json::Value sub_one_req;
    sub_one_req["COMMAND"] = "SUB_1";
    sub_one_req["VALUE"] = 1;

    // Send request, expect value of 0 and successful return code.
    Json::Value sub_one_resp = client.MakeRequest("127.0.0.1", 5000,
	                                              sub_one_req);
	EXPECT_EQ(sub_one_resp["SUCCESS"].asBool(), true);
	EXPECT_EQ(sub_one_resp["VALUE"].asInt(), 0);

	// A "SUB_1" request should now subtract "2" instead.
	request_inst->n_ = 2;
    // Send request, expect value of 0 and successful return code.
    Json::Value sub_two_resp = client.MakeRequest("127.0.0.1", 5000,
                                                  sub_one_req);
    EXPECT_EQ(sub_two_resp["SUCCESS"].asBool(), true);
    EXPECT_EQ(sub_two_resp["VALUE"].asInt(), -1);
}

/// This test came about to address an error in which the server (then using
/// the berkley sockets API) would suddenly begin to return -1 on writes after
/// approximately 2000 of them. Here, we simply set up a number of clients and
/// a number of servers, issue a large number of requests between them, and
/// ensure that it does not hang or fail.
TEST(ServerMiscellaneous, Overflow) {
    Server<RequestClassMethod, RequestClass> *ts1, *ts2, *ts3, *ts4, *ts5,
            *ts6;
    auto *request_inst = new RequestClass(1);
    std::map<std::string, RequestClassMethod> commands {
            {"ADD_1", std::mem_fn(&RequestClass::add_n)},
            {"SUB_1", std::mem_fn(&RequestClass::sub_n)}
    };
    ts1 = new TestServer(5000, commands, request_inst);
    ts2 = new TestServer(5001, commands, request_inst);
    ts3 = new TestServer(5002, commands, request_inst);
    ts4 = new TestServer(5003, commands, request_inst);
    ts5 = new TestServer(5004, commands, request_inst);
    ts6 = new TestServer(5005, commands, request_inst);

    ts1->RunInBackground();
    ts2->RunInBackground();
    ts3->RunInBackground();
    ts4->RunInBackground();
    ts5->RunInBackground();
    ts6->RunInBackground();

    std::vector<Client*> clients(6, new Client());

    Json::Value sub_one_req;
    sub_one_req["COMMAND"] = "SUB_1";
    sub_one_req["VALUE"] = 1;

    for(int j = 0; j < 5000; j++) {
        for(auto & client : clients) {
            std::cout << "Sending req to port " << 5000 + j % 6 << std::endl;
            client->MakeRequest("127.0.0.1", 5000 + j % 6, sub_one_req);
        }
    }

	// If the program has made it this far, then everything works.
	EXPECT_EQ(true, true);
}

/// This test tests both the functionality of "Server::is_alive" and the
/// "Server::Kill" method.
TEST(Client, AliveAndDead) {
    std::map<std::string, RequestClassMethod> commands{};
    auto request_inst = new RequestClass(1);
    Server<RequestClassMethod, RequestClass> server_inst(5001, commands, request_inst);
    server_inst.RunInBackground();
    Client req_maker_inst;

    // Spin until server enters "accept" loop.
    std::this_thread::sleep_for(10ms);

    EXPECT_TRUE(req_maker_inst.IsAlive("127.0.0.1", 5001));
    server_inst.Kill();
    // Spin until server exits "accept" loop.
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(req_maker_inst.IsAlive("127.0.0.1", 5001));
}