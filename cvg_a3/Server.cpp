/*
 * Server.cpp
 *
 *  Created on: Oct 20, 2014
 *      Author: costin
 *
 */

#include "Server.h"

/*
 * Plan is to post this on gitHub, so here's what this does since the documentation leaves it as an exercise for the reader.
 *
 * Initializer creates a new async session (acceptor)
 * 	Now, if you read the docs, the callback gets copied and instantiated as many times as needed.
 * 	The builder session itself will never be destroyed (unless you exit) and once it does something:
 * 		- accept
 * 		- die (it won't without first creating a new instance! It has to accept something in order for something to disconnect)
 * 	it calls the handler, so:
 * 		a) handler creates a new session to be ready for more connections
 * 		b) session ended so destroy the session the handler *instance* was bound to, the handler will always be bound to things that were spawned by the callback,
 * 			the initial connection was not, so if everyone disconnects, the initial one will still listen and on connect will continue the process.
 *
 * 	It's a bit confusing, but think about it logically, it will sink in at some point.
 *
 */

Server::Server(boost::asio::io_service& io_service, std::string ip, short port, std::string status, std::string net_dir) :
		//build inits
		io_service_(io_service), acceptor_(io_service,
				tcp::endpoint(boost::asio::ip::address::from_string(ip), port))
{
	// create new log
	server_log = new LogHandler();
	//store info about server into the log
	server_log->serverData(ip, boost::lexical_cast<std::string>(port), status, net_dir);
	/*#####################
	 * Daemons
	 * ####################*/

	//transaction manager
	t = new TransactionManager(io_service_, *server_log);
	//replication manager
	r = new ReplicationManager(io_service_, *server_log);

	//---------------------------------------------------------------------------------
	//create an end_point object
	tcp::acceptor::endpoint_type *end_type = new tcp::acceptor::endpoint_type;
	// create a new session ptr
	Session* new_Session = new Session(
			// io service, pointer to log manager, pointer to replication manager
			io_service_, *server_log, *r);

	// dispatch socket
	acceptor_.async_accept(new_Session->socket(),
			//endpoint
			*end_type,
			//bind to callback
			boost::bind(&Server::handle_accept, this, new_Session,
					boost::asio::placeholders::error, end_type));
}

void Server::handle_accept(Session* new_Session,
		const boost::system::error_code& error, tcp::acceptor::endpoint_type *end_type)
{
	std::string sClientIp = end_type->address().to_string();
	unsigned short uiClientPort = end_type->port();
	//std::cout<<"Connection from:"<<sClientIp<<" | "<<uiClientPort<<"\n";
	if (!error)
	{
		//the connection that CREATED this handler accepted a session, so start() (no error)
		new_Session->start();

		tcp::acceptor::endpoint_type *end_type = new tcp::acceptor::endpoint_type;
		//now, make a new session and have it ready to accept and bind a NEW handler to it
		new_Session = new Session(io_service_, *server_log, *r);

		// dispatch socket
		acceptor_.async_accept(new_Session->socket(),
				//endpoint
				*end_type,
				//bind to callback
				boost::bind(&Server::handle_accept, this, new_Session,
						boost::asio::placeholders::error, end_type));
	}
	else //session BOUND to this handler died
	{
		delete new_Session;
		delete end_type;
	}
}

