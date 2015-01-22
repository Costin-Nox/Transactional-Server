/*
 * TransactionManager.h
 *
 *  Created on: Oct 30, 2014
 *      Author: costin
 */

#ifndef TRANSACTIONMANAGER_H_
#define TRANSACTIONMANAGER_H_
#include "ServerTools.h"
#include "LogHandler.h"

class TransactionManager
{

public:
	TransactionManager(boost::asio::io_service& io, LogHandler &log);

	~TransactionManager();

	void check();

private:
	int timer_since=0;
	LogHandler* log_ptr;
	boost::asio::deadline_timer timer_;
};


#endif /* TRANSACTIONMANAGER_H_ */
