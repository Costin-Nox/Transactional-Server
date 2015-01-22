/*
 * LogHandler.h
 *
 *  Created on: Oct 24, 2014
 *      Author: costin
 */

#ifndef LOGHANDLER_H_
#define LOGHANDLER_H_
#include "ServerTools.h"

typedef std::pair<std::string, std::string> LogPair;

class LogHandler : ServerTools
{

public:
	LogHandler();
	virtual ~LogHandler();

	bool	commit(std::string header, std::string &code);
	bool	timeouts();
	int		newTxn(std::string data);
	bool 	addLogEntry(std::string header, std::string data, std::string &code);
	void 	printsentries(int i);
	bool 	save();
	bool	canCommit(int tsn, int commit_seq, std::vector<int> &fetch);
	bool	writeFile(int tsn);
	bool	readFile(std::string file_path, std::string &output);
	bool	abort(std::string header, std::string &code);
	void	serverData(std::string ip, std::string port, std::string primary, std::string net_dir);
	void	backupServerData(std::string data);
	bool	makeTxn(std::string data, int txn);

	//get commit as messages
	void getCommits(std::vector<std::string> &holder);
	//shared log;
	std::map<int, std::map<int, LogPair> > log_trans; // <transaction#, <seq#, (header, data)>>
	//log file name, the . before makes it hidden
	const char* fileName 		= ".server.log";
	//recovery log filename, not hidden
	const char* fileNameRecover = "server.old";
	//some locks
	boost::mutex logFlush;
	// read/write lock
	boost::shared_mutex _access;


	//#########################################################################################################################
	//##########									LOG METHODS
	//=========================================================================================================================
	std::string getFileName(int tsn) {
		return log_trans.find(tsn)->second.find(0)->second.second;
	}
	std::string getData(int tsn, int seq) {
		return log_trans.find(tsn)->second.find(seq)->second.second;
	}
	std::string getHeader(int tsn, int seq) {
		return log_trans.find(tsn)->second.find(seq)->second.first;
	}
	std::string getStatus(int tsn) {
		return log_trans.find(tsn)->second.find(-1)->second.first;
	}
	std::string getPortThis() {
		return log_trans.find(0)->second.find(0)->second.second;
	}
	std::string getIpThis() {
		return log_trans.find(0)->second.find(0)->second.first;
	}
	std::string getStatus() {
		return log_trans.find(0)->second.find(-1)->second.first;
	}

	std::string getNetDir() {
		return log_trans.find(0)->second.find(-1)->second.second;
	}

	std::string getStatusZ(int tsn) {
			return log_trans.find(tsn)->second.find(0)->second.first;
	}

	int getStatusData(int tsn) {
		return atoi(log_trans.find(tsn)->second.find(-1)->second.second.c_str());
	}

	void changeStatus(int tsn, std::string status) {
		log_trans.find(tsn)->second.find(-1)->second.first = status;
	}

	void changeStatusZ(int tsn, std::string status) {
			log_trans.find(tsn)->second.find(0)->second.first = status;
	}

	void changeStatusData(int tsn, std::string data) {
		log_trans.find(tsn)->second.find(-1)->second.second = data;
	}

	void deleteBackup() {
		auto it = log_trans.find(-1);
		log_trans.erase(it);
	}

	bool backupRegistered() {
		auto it = log_trans.find(-1);
		return (it != log_trans.end());
	}
	std::string getIpBackup() {
		return log_trans.find(-1)->second.find(0)->second.first;
	}
	std::string getPortBackup() {
		return log_trans.find(-1)->second.find(0)->second.second;
	}

};
#endif


