/*
 * TransactionManager.cpp
 *
 *  Created on: Oct 30, 2014
 *      Author: costin
 */

#include "TransactionManager.h"

TransactionManager::TransactionManager(boost::asio::io_service& io,
		LogHandler &log) : timer_(io, boost::posix_time::seconds(5))
{
	log_ptr = &log;
	timer_.async_wait(boost::bind(&TransactionManager::check, this));
}

void TransactionManager::check()
{
	timer_since++;
	if(timer_since == 30) {
		std::cout << "\n[TM] Checking log\n\t[INFO] I check the log every 2 minutes and delete\n\ttransactions that haven't had any activity since last check" << "\n";
		log_ptr->timeouts();
		timer_since = 0;
	}
		log_ptr->save();
		timer_.expires_at(
				timer_.expires_at() + boost::posix_time::seconds(5));
		timer_.async_wait(boost::bind(&TransactionManager::check, this));

}

TransactionManager::~TransactionManager()
{
	// TODO Auto-generated destructor stub
}

