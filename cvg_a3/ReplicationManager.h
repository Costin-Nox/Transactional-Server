/*
 * ReplicationManager.h
 *
 *  Created on: Nov 21, 2014
 *      Author: costin
 */

#ifndef REPLICATIONMANAGER_H_
#define REPLICATIONMANAGER_H_
#include "ServerTools.h"
#include "Client.h"
#include "LogHandler.h"
#include <queue>

class ReplicationManager : public ServerTools
{

public:
	ReplicationManager(boost::asio::io_service& io, LogHandler &log);
	~ReplicationManager();
	void check();
	void enqueue(std::string header, std::string data) {
		if(log_ptr->getStatus() == "primary") {
			syncQueue.push(std::make_pair(header, data));
		}
	}
	bool syncEmpty() { return syncQueue.empty(); }
	bool loadPrimary();
	bool commit(std::string header);
	bool integrityCheck();
	bool switchToBackup();

private:
	bool switchToPrimary();
	std::string prim_ip, prim_port;
	void primaryOps();
	void backupOps();
	bool alive;
	bool triedConnect;
	Client* client;
	LogHandler* log_ptr;
	boost::asio::deadline_timer timer_;
	std::queue<std::pair<std::string, std::string>> syncQueue;
	int tries;
	bool show_warning;
	bool backup_did_sync;
	bool restarted;
};
#endif /* REPLICATIONMANAGER_H_ */
