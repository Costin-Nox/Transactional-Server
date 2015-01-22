/*
 * Client.h
 *
 *  Created on: Nov 20, 2014
 *      Author: costin
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include "ServerTools.h"

enum { max_length = 1024 };

class Client : public ServerTools
{
public:
	Client(boost::asio::io_service& io_service, std::string ip, std::string port);

	bool connected() { return connect;}
	bool alive();
	bool authenticate();
	std::string sendReceive(std::string toSend);
	bool sendPacket(std::string toSend);
	bool connectSocket();
	bool verbose;

	//Communication
	std::string genRequest(std::string method, std::string txn,	std::string seq, std::string data);
	//overloads:
	std::string genRequest(std::string method, std::string data);
	std::string genRequest(std::string method);
	//generate mirrored packet (ie: combine header + data)
	std::string genEcho(std::string header, std::string data);

	//close socket
	boost::system::error_code closeSocket() {
		boost::system::error_code ec;
		s.close(ec);

		return ec;
	}

	//change port and ip to connect to
	void changeConnection(std::string ip_new, std::string port_new) { ip = ip_new; port = port_new;}

	/*
	 * TODO:
	 * - ask for missing?
	 * - send missing
	 * - attempt commit
	 * - atempt abort
	 * - does txn exist? -- irrelevant
	 */

	virtual ~Client();

private:
	boost::asio::io_service& io_service_;
	std::string ip, port;
	boost::asio::ip::tcp::socket s;
	bool connect;
};

#endif /* CLIENT_H_ */
