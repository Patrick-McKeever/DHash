#ifndef SERVER_H_CHORD_FINAL
#define SERVER_H_CHORD_FINAL
/**
 * server.h
 *
 * This file aims to implement the classes necessary to run a server with a set
 * of commands. Other files should be able to:
 *      - Instantiate a server class with a map of strings to handlers, handlers
 *        being member functions of another class (a class for which the server
 *        maintains an up-to-date pointer);
 *      - Run the server in a way such that it can access that other class'
 *        values and thereby call the appropriate handlers for requests without
 *        the class in question needing to implement any networking IO of its
 *        own.
 *
 * To accomplish this, we will create two template classes, each with two
 * template parameters. The template parameters are:
 *      - RequestHandler : the type of the member functions (std::mem_fn)
 *                         that will handle requests and produce JSON responses.
 *      - RequestClass   : the class to which RequestHandlers belong, and on
 *                         which they will be called.
 *
 * These template parameters will be used to implement two template classes:
 *      - Session : To handle a single connection with a client.
 *      - Server  : To accept multiple connections with multiple clients and
 *                  create/run new sessions for each of them.
 *
 * Due to undefined behavior of the berkeley sockets API (sys/sockets.h),
 * I have chosen to implement network IO through the boost::asio library.
 */

#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <json/json.h>
#include <chrono>
#include <deque>

using boost::asio::ip::tcp;
using boost::system::error_code;


/**
 * The "Session" class is intended to handle a single connection to a server.
 * Upon receiving a connection, it should:
 *      - Read from a data buffer;
 *      - Parse that info as JSON;
 *      - Identify the "COMMAND" field from that JSON request and the corresp-
 *        onding lambda in the "commands_" map;
 *      - Call the lambda, a function of type Request Handler and a member
 *        function of class RequestClass, which will produce a JSON response or
 *        throw an error;
 *      - Return to the client either the JSON response from the handler
 *        or a JSON response indicating error.
 *
 * @tparam RequestHandler The type of the handlers to respond to requests.
 * @tparam RequestClass The type that will run the server and on which
 *                      server commands of type RequestHandler will be called.
 */
template<class RequestHandler, class RequestClass>
class Session :
        public std::enable_shared_from_this<Session<RequestHandler,
                RequestClass>>
{
public:
    typedef std::map<std::string, RequestHandler> CommandMap;

	/**
	 * Constructor.
	 *
	 * @param socket Socket from which to read and write data.
	 * @param commands Map of string to commands of type
	 *                 RequestClass::RequestHandler.
	 * @param request_class_inst The instance of RequestClass on which commands
	 *                           will be called.
	 */
    Session(tcp::socket socket, CommandMap commands,
            RequestClass *request_class_inst)
        : socket_(std::move(socket))
        , commands_(std::move(commands))
        , request_class_inst_(std::move(request_class_inst))
        , reader_((new Json::CharReaderBuilder)->newCharReader())
    {}

	/**
	 * Run a session - i.e. read from the socket, generate a response, and write
	 * said response to the socket.
	 */
    void Run()
    {
        DoRead();
    }

private:
	/// Socket from which to read/write data.
    tcp::socket socket_;
	/// Instance on which commands will be called.
    RequestClass *request_class_inst_;
	/// Map of strings to commands.
    CommandMap commands_;
    /// Reads JSON.
    const std::unique_ptr<Json::CharReader> reader_;
    /// Writes JSON.
	Json::StreamWriterBuilder writer_;
	/// Buffer into which session will read socket info.
	char data_[2048];
	/// String to store server response (must be data member so it
	/// can outlast the duration of DoWrite, since async_write returns immed-
	/// iately).
    std::string resp_;

	/**
	 * Read a single request from the socket.
	 * If there are no errors, write the appropriate response.
	 */
    void DoRead()
    {
        auto self(this->shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_),
                                [this, self](error_code ec, std::size_t length)
                                {
                                  if (!ec)
                                      DoWrite(length);
                                });
    }

	/**
	 * After request has been read from socket_ into data_, read data_, generate
	 * response, and write it to the socket.
	 * @param length Length of data read from socket.
	 */
    void DoWrite(std::size_t length)
    {
        JSONCPP_STRING parse_err;
        Json::Value json_req, json_resp;
        std::string client_req_str(data_);

        if (reader_->parse(client_req_str.c_str(),
                           client_req_str.c_str() +
                           client_req_str.length(),
                           &json_req, &parse_err))
        {
            try {
                // Get JSON response.
                json_resp = ProcessRequest(json_req);
                json_resp["SUCCESS"] = true;
            } catch (const std::exception &ex) {
                // If json parsing failed.
                json_resp["SUCCESS"] = false;
                json_resp["ERRORS"] = std::string(ex.what());
            }
        } else {
            // If json parsing failed.
            json_resp["SUCCESS"] = false;
            json_resp["ERRORS"] = std::string(parse_err);
        }

        resp_ = Json::writeString(writer_, json_resp);

        auto self(this->shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(resp_),
                                 [this, self]
                                         (boost::system::error_code ec,
                                          std::size_t bytes_xfered) {
                                   if (!ec)     DoRead();
                                 });
    }

	/**
	 * Lookup command specified in request in commands map. Call it,
	 * and return its response.
	 *
	 * @param request Request issued by client.
	 * @return Response to request.
	 */
    Json::Value ProcessRequest(Json::Value request)
    {
        Json::Value response;
        std::string command = request["COMMAND"].asString();

        // If command is not valid, give a response with an error.
        if(commands_.find(command) == commands_.end())
			throw std::runtime_error("Invalid command.");

        // Otherwise, run the relevant handler.
        else {
            RequestHandler handler = commands_.at(command);
            response = handler(*request_class_inst_, request);
        }
        return response;
    }

};

/**
 * The "Server" class is intended to accept multiple connections and generate
 * new sessions for each of them.
 *
 * @tparam RequestHandler The type of the handlers to respond to requests.
 * @tparam RequestClass The type that will run the server and on which
 *                      server commands of type RequestHandler will be called.
 */
template <class RequestHandler, class RequestClass> class Server {
public:
    using CommandMap = std::map<std::string, RequestHandler>;

	/**
	 * Constructor.
	 *
	 * @param port Port on which server will be run.
	 * @param commands Map of strings to lambdas (will be passed to Session
	 *                 constructor).
	 * @param request_class_inst Instance on which command methods will be
	 *                           called (also passed to Session constructor).
	 */
    Server(uint16_t port, CommandMap commands, RequestClass *request_class_inst)
        : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port))
        , commands_(std::move(commands))
        , request_class_inst_(std::move(request_class_inst))
    {
		// NOTE: This won't start running until we run the io_context.
        DoAccept();
    }

	/**
	 * Destructor. Join the server threads.
	 */
    ~Server()
    {
        if (t_.joinable())
            t_.join();
        assert(not t_.joinable());
    }

	/**
	 * Run the io_context and thereby start the server.
	 */
    void Run()
    {
        io_context_.run();
    }

	/**
	 * Run the server as a thread.
	 * NOTE: We can't detach this thread. It screws up synchronization of
	 *       the io_context between threads, which causes race conditions and,
	 *       ultimately, segfaults.
	 */
    void RunInBackground()
    {
        if (!t_.joinable()) {
            t_ = std::thread([this] {
              Run();
              std::cout << "THREAD EXIT" << std::endl;
            });
        }
    }

	/**
	 * Kill the server by closing acceptor_.
	 */
    void Kill()
    {
		// tcp::acceptor::close is not thread-safe, so we must instead tell the
		// io_context to close the acceptor as soon as it's able to do so.
        post(io_context_, [this] {
          std::cout << "CLOSING" << std::endl;
          acceptor_.close(); // causes .cancel() as well
        });
    }

private:
	/// The server starts on io_context_.run().
    boost::asio::io_context io_context_;
	/// Acceptor accepts new connections from clients.
    tcp::acceptor acceptor_;
	/// Maps strings of commands to associated member funcs of RequestClass.
    CommandMap commands_;
	/// The instance of RequestClass on which member funcs will be called.
    RequestClass *request_class_inst_;
	/// The thread on which we will run the server.
    std::thread t_;

	/**
	 * Accept a single connection, setup a connection, and run said connection.
	 */
    void DoAccept()
    {
        acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket) {
                  if (ec) {
                      std::cout << "Accept loop: " << ec.message() << std::endl;
                  } else {
					  tcp::endpoint client_ept = socket.remote_endpoint();
                      std::make_shared<Session<RequestHandler, RequestClass>>(
                              std::move(socket), commands_, request_class_inst_)
                              ->Run();
                      DoAccept();
                  }
                });
    }
};

#endif