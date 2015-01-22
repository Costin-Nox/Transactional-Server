/*
 * Server.h
 *
 *  Created on: Oct 20, 2014
 *      Author: costin
 */

#ifndef SERVER_H_
#define SERVER_H_
#include "Session.h"
#include "TransactionManager.h"
#include "ReplicationManager.h"
using boost::asio::ip::tcp;

class Server
{
public:
	Server(boost::asio::io_service& io_service, std::string ip, short port, std::string status, std::string net_dir);
	void handle_accept(Session* new_Session,
			const boost::system::error_code& error, tcp::acceptor::endpoint_type *end_type);

	LogHandler* server_log;
	TransactionManager* t;
	ReplicationManager* r;

private:
	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
};

#endif /* SERVER_H_ */
