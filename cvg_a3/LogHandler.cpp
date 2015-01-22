/*
 * LogHandler.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: costin
 */

#include "LogHandler.h"

LogHandler::LogHandler()
{
	/*
	 * There's no "clean exit" so.. any exit of the server is a bad exit.
	 * Now that we got that figured out, this means that we need to restore state
	 * if there was one on every program load.
	 *
	 * i could binary this, save some space, but for the sake of proof
	 * i wont so the log can be opened and be somewhat readable..
	 */

	//STEP1: check if there's a server.log in the current working folder
	std::string result;
	if (checkPath(fileName, result))
	{
		if (result == "file")
		{ // found the bugger!
			std::cout << "[-] LOG data found.. attempting to load state\n";
			//STEP2: load the file
			try
			{
				std::ifstream ifs(fileName);
				boost::archive::text_iarchive in_file(ifs);
				//STEP3: shove data back where the sun don't shine (no sun in C++.. is very sad..)
				in_file >> log_trans;
				std::cout << "[+] Much load, such data, many byte, wow!\n\tLoad successful - possible it's empty\n\n";
				//STEP4: consistency check, dispatch some transaction timeouts..etc

				//Loading failed -> corrupted log
			} catch (boost::archive::archive_exception &e)
			{
				std::cout << "[!] CRITICAL ERROR: log data corrupted\n";
				int ret_code = std::rename(fileName, fileNameRecover);
				//rename code
				if (ret_code == 0)
				{
					std::cout
					<< "[!] File was renamed to server.old, recovery of state impossible without fixing it.\n"
					<< "Execution will continue with new log file, current server data integrity might be compromised!\n";
				}
				else
				{
					std::cerr << "[!] Error during the rename: "
							<< ret_code
							<< '\n';
					std::cerr << "[!] Execution cannot continue. \n";
					exit(-1);
				}
			}
		}
	}
	else
	{
		std::cout
		<< "[+] No log data present in current working directory,\n\t one will be generated..\n";
		save();
	}
}

//#########################################################################################################################
//##########									METHODS
//=========================================================================================================================
/* [INFO] Locking and Race conditions:
 *
 * Lock is most likely pointless for the save() fn. Using implicit strand:
 * http://www.boost.org/doc/libs/1_56_0/doc/html/boost_asio/overview/core/strands.html
 *
 * I'm not explicitly running any threads which would create race conditions, io_service.run() should
 * take care of eliminating race conditions within its scope.
 *
 * Yet, i'm using a lock anyway. Why? Well.. i changed this code a few times since making that decision,
 * the way i was doing it, didn't *really* ensure that if multiple saves happened they wouldn't be interleaved..
 * bleh.. i can't explain this without the code, so let's just say i left the lock in here for peace of mind
 * and making the marker happy :P
 *
 */

bool LogHandler::save() {
	logFlush.lock();
	//critical area -->

	//long story short, no fsync, this should be an equivalent, more graceful way of doing it with boost.
	boost::filesystem::path filePath(fileName);
	boost::iostreams::stream<boost::iostreams::file_descriptor_sink> file(filePath);
	//write stuff
	boost::archive::text_oarchive output_file(file);
	output_file << log_trans;

	//  Ensure the buffer's written to the OS ...
	if (file.rdbuf())
	{
		file.rdbuf()->pubsync();
	}

	//  Tell the OS to sync it with the real device.
	::fdatasync(file->handle());
	//end ------------>
	logFlush.unlock();
	//std::cout<<"Log saved\n";
	return true;
}




/* ----------------------------------------------------------------------------------------
 * HOW:
 *
 * Is called by Transaction Manager every few minutes
 *
 */

bool LogHandler::timeouts() {
	std::string status;
	int at;


	for (auto it_tsn = log_trans.begin(); it_tsn != log_trans.end(); ++it_tsn)
	{

		if(it_tsn->first >= 0 && it_tsn->second.find(-1) != it_tsn->second.end()) {

			status = it_tsn->second.find(-1)->second.first;
			at = it_tsn->first;

			if(status == "ACTIVE") {
				changeStatus(at, "FLAG");
			} else if (status == "FLAG") {
				changeStatus(at, "EXPIRED");
			} else if(status == "EXPIRED") {
				log_trans.erase(it_tsn);
				std::cout<<"[TM] transaction deleted: "<<at<<"\n";
			} else if(status == "COMMITTED") {
				//do nothing
				/*
				if(getStatusZ(at) != "EXPIRED") {
					changeStatusZ(at, "EXPIRED");
				} else if(getStatusZ(at) == "EXPIRED") {
					log_trans.erase(it_tsn);
					std::cout<<"[TM] transaction deleted: "<<at<<"\n";
				}
				*/
			} else if(status == "WAITING") {
				if(getStatusZ(at) == "FLAG") {
					changeStatusZ(at, "EXPIRED");
				} else if(getStatusZ(at) == "EXPIRED") {
					log_trans.erase(it_tsn);
					std::cout<<"[TM] transaction deleted: "<<at<<"\n";
				} else {
					changeStatusZ(at, "FLAG");
				}
			}
			else if ((status == "WRITING" || status == "backup" || status == "primary"))
			{
				//do not touch
			}
			else
			{
				changeStatus(at, "FLAG");
			}
		}
	}

	save();

	return (true);
}

void LogHandler::getCommits(std::vector<std::string> &holder) {
	std::string status;
	int at;


	for (auto it_tsn = log_trans.begin(); it_tsn != log_trans.end(); ++it_tsn)
	{

		if(it_tsn->first >= 0 && it_tsn->second.find(-1) != it_tsn->second.end()) {
			status = it_tsn->second.find(-1)->second.first;
			at = it_tsn->first;
			if(status == "COMMITTED") {
				std::string toPush="COMMIT "+iToS(at)+" "+iToS(getStatusData(at))+" 0";
				holder.push_back(toPush);
			}
		}
	}
}

//#########################################################################################################################
//##########									WRITE
//=========================================================================================================================
/*
 * add log entry after packet is received, &code will be updated with message for packet;
 * returns false if it fails to add entry
 */
bool LogHandler::addLogEntry(std::string header, std::string data,
		std::string &code)
{
	/* ###################
	 * # CHECK CONDITIONS
	 * ----------------------------------------------------------------------------------------------------------------------*/
	int tsn = getTsn(header);
	//check if sequence number is in valid range
	if(getSeq(header) < 1) {
		code = genAnswerStr("ERROR", header, "204",
				"Sequence number invalid: must be 1 or larger");
		printsentries(1);
		return false;
	}
	//check if transaction is valid
	if (log_trans.find(tsn) == log_trans.end())
	{
		code = genAnswerStr("ERROR", header, "201", "Transaction ID not found");
		return false;
	}
	//check if transaction was committed
	if (getStatus(tsn) == "COMMITTED")
	{
		code = genAnswerStr("ERROR", header, "202",
				"Transaction was previously committed");
		return false;
	}
	//check if transaction expired
	if (getStatus(tsn) == "EXPIRED")
	{
		code = genAnswerStr("ERROR", header, "220",
				"Transaction expired, please start a new one.");
		return false;
	}
	//check if sequence no exists already under this transactions, if it does, ignore it and warn
	if (log_trans.find(tsn)->second.find(getSeq(header)) != log_trans.find(tsn)->second.end())
	{
		code = genAnswerStr("WARNING", header, "301",
				"SeqNum for current tsnID already received, ignored");
		return false;
	}
	//check if transaction is waiting a commit and this write is higher than commit write
	if ( getStatus(tsn) == "WAITING" && getStatusData(tsn) < getSeq(header))
	{
		code = genAnswerStr("ERROR", header, "202",
				"Sequence number for write is higher than pending commit. Ignored.");
		return false;
	}

	/* ###############
	 * # Write to log
	 * ----------------------------------------------------------------------------------------------------------------------*/

	log_trans.find(tsn)->second.insert(
			std::make_pair(getSeq(header), std::make_pair(header, data)));


	//check if it's a transaction waiting for commit, might be last needed packet to do it
	if (getStatus(tsn) == "WAITING")
	{
		std::vector<int> t;
		if(canCommit(tsn, getStatusData(tsn), t)) {
			//fake commit header
			std::string s = "empty\t" + iToS(tsn) + "\t" + iToS( getStatusData(tsn) ) + "\t0\t";
			return commit(s, code);
		}
	} else {
		changeStatus(tsn, "ACTIVE");
	}

	return true;
}
//#########################################################################################################################
//##########									NEW TRANSACTION
//=========================================================================================================================
int LogHandler::newTxn(std::string data)
{

	if(!boost::filesystem::portable_posix_name(data)) {
		return 0;
	}
	int rn = randomNum(100000);
	//make sure tsn was not assigned
	while (log_trans.find(rn) != log_trans.end())
	{
		rn = randomNum(100000);
	}

	log_trans[rn].insert(std::make_pair(-1, std::make_pair("ACTIVE", "0")));
	log_trans[rn].insert(std::make_pair( 0, std::make_pair("NEW_TXN", data)));

	save();

	//return generated transaction number
	return rn;
}

bool LogHandler::makeTxn(std::string data, int txn)
{

	if(log_trans.find(txn) == log_trans.end()) {
		log_trans[txn].insert(std::make_pair(-1, std::make_pair("ACTIVE", "0")));
		log_trans[txn].insert(std::make_pair( 0, std::make_pair("NEW_TXN", data)));
		std::cout<<"MAKE_TXN:"<<data<<"\n\n";
		save();

		//return generated transaction number
		return true;
	}
	else
	{
		return false;
	}
}
//Like a transaction, basically store server state inside a transaction;
void LogHandler::serverData(std::string ip, std::string port, std::string primary, std::string net_dir)
{
	auto it = log_trans.find(0);
	if(it != log_trans.end()){
		log_trans.erase(it);
	}

	log_trans[0].insert(std::make_pair(-1, std::make_pair(primary, net_dir)));
	log_trans[0].insert(std::make_pair( 0, std::make_pair(ip, port)));

}
//Backup server location
void LogHandler::backupServerData(std::string data)
{
	std::vector<std::string> strs;
	boost::split(strs, data, boost::is_any_of("|"));
	log_trans[-1].insert(std::make_pair( 0, std::make_pair(strs[0], strs[1])));
}

//#########################################################################################################################
//##########									COMMIT
//=========================================================================================================================
bool LogHandler::commit(std::string header, std::string &code)
{

	/* ###################
	 * # CHECK CONDITIONS
	 * ----------------------------------------------------------------------------------------------------------------------*/
	int tsn = getTsn(header);
	//check if transaction is valid
	if (log_trans.find(tsn) == log_trans.end())
	{
		code = genAnswerStr("ERROR", header, "201", "Transaction ID not found");
		return false;
	}
	//check if transaction was committed
	if (getStatus(tsn) == "COMMITTED" && getStatusData(tsn) == getSeq(header))
	{
		code = genAnswerStr("ACK", header, "0", "Already committed");
		return false;
	}
	//check if transaction expired
	if (getStatus(tsn) == "EXPIRED")
	{
		code = genAnswerStr("ERROR", header, "220",
				"Transaction expired, please start a new one.");
		return false;
	}
	//check if transaction is waiting a commit
	if ( (getStatus(tsn) == "WAITING" || getStatus(tsn) == "COMMITTED" ) && getStatusData(tsn) != getSeq(header))
	{
		code = genAnswerStr("ERROR", header, "202",
				"Transaction was previously committed with a different sequence number");
		return false;
	}

	/* Wiki said i shouldn't check this, so i disabled this one..
	//check if the commit sequence number matches last write
	int seq_no_max;
	for (auto it_log  = log_trans.find(tsn)->second.cbegin();
			it_log != log_trans.find(tsn)->second.cend();
			++it_log)
	{
		seq_no_max = it_log->first;
	}

	if (seq_no_max > getSeq(header))
	{
		code = genAnswerStr("WARNING", header, "403",
				"WRITE request with a higher SEQ nr exists! Expected: "+boost::lexical_cast<std::string>(seq_no_max));
		return false;
	}
	*/

	else if(getSeq(header) < 1)
	{
		code = genAnswerStr("WARNING", header, "404",
				"Nothing to commit, request ignored");
		return false;
	}

	/*----------------------------------------------------------------------------------------------------------------------------
	 * ##############
	 * ## WRITE
	 * ##############
	 *
	 * at this point we can look into committing the transaction,
	 * need to make sure we have all the data we should first though
	 */

	/*
	 * CASE : sequence received is bigger than any recorded -> missing packets
	 */
	std::vector<int> toSend;

	if(canCommit(tsn, getSeq(header), toSend)) //done
	{
		//set this to waiting, change to COMITTED when write op completes.
		changeStatus(tsn, "WRITING");
		changeStatusData(tsn, boost::lexical_cast<std::string>(getSeq(header)));
		//flush log to disk in waiting state before attempting to write;
		save();
		if(getStatus() == "primary")  { //if running as backup server, wait for backup to commit first
			code = "committed";
			return false;
		} else { // running as backup, so go ahead and try to commit.
			if(writeFile(tsn)) {
				changeStatus(tsn, "COMMITTED");
				code = "committed";
			} else {
				code = genAnswerStr("ERROR", header, "205", "commit fail, i/o error");
				changeStatus(tsn,"WAITING");
			}
			save();
			return false;
		}
	}
	else //missing data
	{
		//set to waiting
		changeStatus(tsn,"WAITING");
		//set number of highest write expected
		changeStatusData(tsn, boost::lexical_cast<std::string>(getSeq(header)));
		//incomplete no of packets
		code = "incomplete";
		save();
		return false;
	}


	//---------------------------------------------------------------------------------------------------------------------------
	code = "[FATAL ERROR] Something went terribly wrong.\n";
	return false;
}

//#########################################################################################################################
//##########									READ FROM DISK
//=========================================================================================================================

bool LogHandler::readFile(std::string path, std::string &output) {

	bool ret = false;
	//info: locks are scoped, no need to unlock, does it itself :)
	boost::shared_lock<boost::shared_mutex> lock(_access);
	using namespace std;
	string line;
	ifstream myfile (path);
	if (myfile.is_open())
	{
		while ( getline (myfile,line) )
		{
			output= output + line + "\n";
			ret = true;
		}
		myfile.close();

	}
	return ret;

}

//#########################################################################################################################
//##########									WRITE TO DISK
//=========================================================================================================================

bool LogHandler::writeFile(int tsn) {

	try {
		boost::upgrade_lock<boost::shared_mutex> lock(_access);
		boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(lock);
		boost::filesystem::path filePath(getFileName(tsn));

		if (!is_regular_file(filePath)) {
					std::ofstream outfile (getFileName(tsn).c_str());
					outfile.close();
				}

		boost::iostreams::stream<boost::iostreams::file_descriptor_sink> file(filePath, std::ios::app|std::ios::out);

		using namespace std;
		if (file.is_open())
		{
			for(int seq = 1; seq != getStatusData(tsn)+1; seq++) {
				file << getData(tsn, seq) +"\n";
			}

			//  Ensure the buffer's written to the OS ...
			if (file.rdbuf())
			{
				file.rdbuf()->pubsync();
			}

			//  Tell the OS to sync it with the real device.
			::fdatasync(file->handle());


			file.close();
			return true;
		}
	} catch (...) {
		return false;
	}
	return false;

}

//#########################################################################################################################
//##########									ABORT
//=========================================================================================================================

bool LogHandler::abort(std::string header, std::string &code){
	/* ###################
	 * # CHECK CONDITIONS
	 * ----------------------------------------------------------------------------------------------------------------------*/
	int tsn = getTsn(header);
	//check if transaction is valid
	if (log_trans.find(tsn) == log_trans.end())
	{
		code = genAnswerStr("ERROR", header, "201", "Transaction ID not found");
		return false;
	}
	//check if transaction was committed
	if (getStatus(tsn) == "COMMITTED")
	{
		code = genAnswerStr("ERROR", header, "202",
				"Transaction was previously committed");
		return false;
	}
	//check if transaction expired
	if (getStatus(tsn) == "EXPIRED")
	{
		code = genAnswerStr("ERROR", header, "220",
				"Transaction expired.");
		return false;
	}
	/* ###################
	 * # DELETE
	 * ----------------------------------------------------------------------------------------------------------------------*/
	auto it = log_trans.find(tsn);
	log_trans.erase(it);
	std::cout<<"[ABORT] Transaction deleted: "<<tsn<<"\n";

	return true;

}


//#########################################################################################################################
//##########									HELPERS
//=========================================================================================================================

bool LogHandler::canCommit(int tsn, int commit_seq, std::vector<int> &fetch) {
	int current = -1;

	for (auto it_log  = log_trans.find(tsn)->second.begin();
			it_log != log_trans.find(tsn)->second.end();
			++it_log)
	{
		if(current != (it_log->first)-1) {
			for(int i = current+1; i<it_log->first; i++) {
				fetch.push_back(i);
			}
		}
		current = it_log->first;
	}
	if(current < commit_seq) {
		for(int i = current; i<commit_seq; i++) {
			fetch.push_back(i+1);
		}
	}

	return (fetch.empty() && commit_seq != 0);
}

//debug
void LogHandler::printsentries(int i)
{
	std::cout << "\n######## Printing Log Data ############" << std::endl;

	for (auto it_tsn = log_trans.begin(); it_tsn != log_trans.end(); ++it_tsn)
	{
		std::cout << " ** Tsn\t: " << it_tsn->first << std::endl;
		for (auto it_log = it_tsn->second.begin();
				it_log != it_tsn->second.end(); ++it_log)
		{
			std::cout << "   * \tSeq\t: " << it_log->first << " Header: "
					<< it_log->second.first << "\n\tData: "
					<< it_log->second.second << "\n";
		}
	}
	std::cout << "#######################################" << std::endl;
}

LogHandler::~LogHandler()
{
	std::cout<<"[!] Log destructor invoked. (not a good sign)\n";
}

