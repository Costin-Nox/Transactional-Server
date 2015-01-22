/*
 * Client.cpp
 *
 *  Created on: Nov 20, 2014
 *      Author: costin
 */

#include "Client.h"

extern bool debug_client;

Client::Client(boost::asio::io_service& io_service, std::string ip,
		std::string port) :io_service_(io_service), s(io_service), port(port), ip(ip)

{
	//initialize
	connect = false;
	//this just tells it if it should output a message or not
	//info is good, but this prevents spam
	verbose = true;
}

//#########################################################################################################################
//##########									CONNECT Socket
//=========================================================================================================================

bool Client::connectSocket() {
	using boost::asio::ip::tcp;

	try
	{
		if(verbose && debug_client) {
			std::cout << "Trying to connect to: " << ip << " port: " << port<< std::endl;
		}

		tcp::resolver resolver(io_service_);
		boost::asio::ip::tcp::resolver::query query(ip, port);
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query), end;
		boost::system::error_code error_connect;
		boost::asio::connect(s, endpoint_iterator, error_connect);

		if(verbose && debug_client)
			std::cout << boost::system::system_error(error_connect).what()<< std::endl;

		if(!error_connect) {
			verbose = false;
			connect = true;
			sendPacket(genRequest("AUTH"));
			return true;
		} else {
			verbose = true;
			connect = false;
			return false;
		}
	} catch (std::exception& e)
	{
		connect = false;
		std::cout << "[Client.ccp] Exception: " << e.what() << "\n";
		return false;
		//TODO connect FAIL;
	}

}

//#########################################################################################################################
//##########						Generate packets
//=========================================================================================================================

std::string Client::genRequest(std::string method, std::string txn,
		std::string seq, std::string data)
{

	std::string toRet;

	int size_err = data.size();

	toRet = method + " " + txn + " " + seq + " ";
	if (data == " ")
		toRet = toRet + "0" + "\r\n\r\n\r\n";
	else
		toRet = toRet + boost::lexical_cast<std::string>(size_err+1) + "\r\n\r\n"
				+ data + "\n";

	return toRet;
}
//overloads
std::string Client::genRequest(std::string method, std::string data)
{
	return genRequest(method, "-1", "0", data);
}
std::string Client::genRequest(std::string method)
{
	return genRequest(method, "-1", "0", " ");
}
//gen 'echo' packet, ie: forward received packet to backup
std::string Client::genEcho(std::string header, std::string data)
{

	std::string toRet;
	std::vector<std::string> strs;
	boost::split(strs, header, boost::is_any_of("\t "));
	if(strs.size() <3) { //prevent potential seg faults
		strs[2] = '0';
		if(strs.size() <2)
			strs[1] = '0';
	}
	int size_err = data.size();

	toRet = strs[0] + " " + strs[1] + " " + strs[2] + " ";
	if (data.size() < 2)
		toRet = toRet + "0" + "\r\n\r\n\r\n";
	else
		toRet = toRet + boost::lexical_cast<std::string>(size_err+1) + "\r\n\r\n"
				+ data + "\n";

	//std::cout<<"\n##############\nGenerated:\n"<<toRet<<"\n#####################\n\n";

	return toRet;
}

//#########################################################################################################################
//##########									COMMUNICATION
//=========================================================================================================================

//send and receive answer, returns it as a string
std::string Client::sendReceive(std::string toSend)
{
	std::string request = toSend;
	std::string data;
	size_t request_length = request.size();
	boost::asio::write(s, boost::asio::buffer(request, request_length));

	boost::asio::streambuf b;
	request_length = boost::asio::read_until(s, b, "\r\n\r\n");
	std::istream is(&b);
	std::string header;
	//get header
	std::getline(is, header);
	//first blank line
	std::getline(is, data);
	// data attached?
	if (getBytesResponse(header) > 0)
	{
		// yes, read it
		boost::asio::read_until(s, b, "\r\n");
	}
	//if data, get it, if not there was one more empty line anyway
	std::getline(is, data);

	return header + "\t\t\t" + data;
}
//send a packet, ignore answer
bool Client::sendPacket(std::string toSend)
{
	std::string request = toSend;
	std::string data;
	size_t request_length = request.size();
	boost::asio::write(s, boost::asio::buffer(request, request_length));

	return true;
}

Client::~Client()
{
	delete this;
}

