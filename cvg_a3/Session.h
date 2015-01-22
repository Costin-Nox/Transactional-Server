/*
 * Session.h
 *
 *  Created on: Oct 20, 2014
 *      Author: costin
 */
#ifndef SESSION_H_
#define SESSION_H_
#include "LogHandler.h"
#include "ServerTools.h"
#include "ReplicationManager.h"

using boost::asio::ip::tcp;

class Session : public ServerTools
{
public:
  Session(boost::asio::io_service& io_service,
		  //pass a reference to the log handler into each session handler,
		  //the session gets reset and there needs to be a persistent solution for the server to be stateful
		  LogHandler &server_log, ReplicationManager &rm);
  ~Session();
  tcp::socket& socket();

  void start();
  void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
  void handle_write(const boost::system::error_code& error);
  void sendACK_RESEND(int tsn);
  void doCommit(std::string header);

  //helpers
  bool sendACK(std::string s);
  bool sendNACK(std::string s);

private:
  ReplicationManager* rep_man;
  LogHandler* log_ptr;
  tcp::socket socket_;
  enum { max_length = 5000 };
  char data_[max_length];
  boost::asio::streambuf b;
  boost::asio::streambuf c;

  boost::asio::io_service& io_service_;

  boost::shared_mutex _packet, _header;

  std::size_t completion_condition(
		  // Result of latest read_some operation.
		  const boost::system::error_code& error,

		  // Number of bytes transferred so far.
		  std::size_t bytes_transferred,

		  //size of payload
		  int total
  );

  bool executeCommand(int i, std::string header, std::string data, std::string &error);

  //Atuhentication
  void doAuth(std::string ip, unsigned short p) {
	  auth_port = p;
	  auth_ip = ip;
  }

  bool isAuth(std::string ip, unsigned short p) {
	  return (ip == auth_ip && auth_port == p);
  }

private:
  std::string auth_ip;
  unsigned short auth_port;
};

#endif /* SESSION_H_ */
