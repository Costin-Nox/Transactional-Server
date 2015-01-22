/*
 * ReplicationManager.cpp
 *
 *  Created on: Nov 21, 2014
 *      Author: costin
 *  Assumption:
 *  	NO NETWORK PARTITIONS (caps cause i'm serious, it doesnt handle it at all)
 *  	DIFFERENT WORKING DIR BETWEEN SERVER AND BACKUP (caps cause see above)
 *
 *  Description:
 *
 *  	Daemon that checks the queue of packets received and processed by the primary,
 *  	the queue is stored here, processed and sent to the backup. If there is no backup, the queue will
 *  	accumulate and be sent when a backup registers.
 *
 *  	Current implementation supports one backup server.
 *
 *  	It is also in charge of pinging in case of running in backup mode, and trying to find a primary and letting it know it exists.
 *
 *
 */

#include "ReplicationManager.h"
//time to sleep
const int time_ms = 300;
//print debug messages?
extern bool debug_rm;

//////////////////////////////////////
/////// Initializer
//////////////////////////////////////

ReplicationManager::ReplicationManager(boost::asio::io_service& io,	LogHandler &log) :
//time to sleep between operations
timer_(io, boost::posix_time::milliseconds(time_ms))
{
	log_ptr = &log;

	loadPrimary();
	//create a client
	client = new Client(io, "0.0.0.0", "0");
	alive = false;
	show_warning = true;
	triedConnect = true;
	backup_did_sync = false;
	restarted = false;
	tries = 0;
	timer_.async_wait(boost::bind(&ReplicationManager::check, this));
}

//////////////////////////////////////
/////// Async Function
//////////////////////////////////////
/*
 * Gets executed periodically
 */
void ReplicationManager::check()
{
	if (log_ptr->getStatus() == "primary")
	{
		primaryOps();
	}
	else
	{
		backupOps();
	}

	timer_.expires_at(
			timer_.expires_at() + boost::posix_time::milliseconds(time_ms));
	timer_.async_wait(boost::bind(&ReplicationManager::check, this));

}
//#########################################################################################################################
//##########							METHODS For primary/backup behavior
//=========================================================================================================================


//////////////////////////////////////
/////// Primary Server Operations
//////////////////////////////////////

void ReplicationManager::primaryOps()
{

	/*
	 * Connected and queue not empty
	 * ~attempt to push queue to backup
	 */
	if (alive && !syncQueue.empty())
	{
		try {
			if(debug_rm) {std::cout << "[RM] Performing sync \n\n";}

			while (!syncQueue.empty())
			{
				std::string reply_holder = client->sendReceive(client->genEcho(syncQueue.front().first, syncQueue.front().second));
				//print returned message
				if(debug_rm)
				{
					std::cout 	<< "Backup reply:\n"
								<< reply_holder
								<< "\n-----------------\n";
				}
				//remove from queue
				syncQueue.pop();
			}
		//connection failed
		}catch (...) {
			std::cout<<"\n[RM] Backup server connection failed\n";
			alive = false;
		}
	}
	/*
	 * Not connected, but a backsup server has registered
	 * Try to connect to it
	 */
	else if (!alive && log_ptr->backupRegistered())
	{
		std::cout << "[RM] Attempting to connect to backup \n";
		//connection was attempted, might be the info in the client is outdated.
		if(triedConnect) {
			if(debug_rm){std::cout<<"[RM]...updating backup info..\n";}
			client->changeConnection(log_ptr->getIpBackup(), log_ptr->getPortBackup());
		}
		//try to connect
		alive = client->connectSocket();
		if (!alive) {
			std::cout << "[!]Failed.\n";
			log_ptr->deleteBackup();
			triedConnect = true;
		} else {
			integrityCheck();
		}
	}

}

//////////////////////////////////////
/////// Commit Request
//////////////////////////////////////


bool ReplicationManager::commit(std::string header) {


	bool back_up_mode = false;
	//sync the queue -- if there was one
	if (log_ptr->getStatus() == "primary") {
		primaryOps();
	} else {
		//--- running as backup...
		back_up_mode = true;
	}


	int tsn = getTsn(header);

	if (alive && syncQueue.empty())
	{
		if(debug_rm && show_warning)
			std::cout << "[RM] Commit request detected, checking backup server..\n";
		std::string result;
		try {
			if(debug_rm && show_warning)
				std::cout << "[RM] Backup online, waiting for backup to commit..\n";
			result = client->sendReceive(client->genEcho(header, " "));
			std::vector<std::string> strs;
			boost::split(strs, result, boost::is_any_of(" "));
			if(strs[0] =="ACK") {
				if(debug_rm && show_warning)
					std::cout << "[RM] Backup committed\n";
				return true;
			}else if(back_up_mode) {
				printf("[RM] CRITRICAL ERROR, backup contains commits the primary does not!!\n Killing primary.. ");
					client->sendPacket(client->genRequest("KILL"));
					client->closeSocket();
					client->changeConnection("0.0.0.0", "0");
					tries = 4;
					restarted = true;


				return false;

			}else if(strs[0] =="ACK_RESEND") {
				printf("  * Found missing packets, sending them..\n");
				for(int seq = 1; seq != log_ptr->getStatusData(tsn)+1; seq++) {
					enqueue(log_ptr->getHeader(tsn, seq) ,log_ptr->getData(tsn, seq));
				}
				show_warning = false;
				bool result = commit(header);
				show_warning = true;
				if(result) {printf("done!");}
				return result;
			}else if(strs[0]=="ERROR" && strs[3] == "201") {
				printf("  * Found missing transaction, creating it..\n");
				std::string make_tsn_header = "MAKE_TXN "+iToS(tsn)+" -1 0";
				std::string make_tsn_data	= log_ptr->getFileName(tsn);
				enqueue(make_tsn_header, make_tsn_data);

				show_warning = false;
				bool result = commit(header);
				show_warning = true;
				if(result) {printf("done!");}
				return result;
			} else {
				std::cout << "[RM] [ERROR] Backup could not commit..canceling request, here is backup response:\n"<<result<<"\n\n";
				return false;
			}

		//connection failed
		}catch (...) {
			if(show_warning) {std::cout<<"\n[RM] <WARNING> Backup server connection failed.. will commit locally and save to queue\n";}
			enqueue(header, " ");
			alive = false;
			return true;
		}

	} else if(alive && !syncQueue.empty()) {
		return commit(header);
	} else {
		if(show_warning) {std::cout<<"\n[RM] <WARNING> No backup server alive, will commit locally and save to queue\n";}
		enqueue(header, " ");
		return true;
	}
}

//////////////////////////////////////
/////// Backup server
//////////////////////////////////////


void ReplicationManager::backupOps()
{
	client->closeSocket();
	//try to open a socket to primary
	if(client->connectSocket()) {
		//create data containing ip and port for this(running as backup)
		std::string info = log_ptr->getIpThis()+"|"+log_ptr->getPortThis();
		if(!backup_did_sync && !restarted) {
			backup_did_sync = true;
			integrityCheck();
		} else {
			//send a ping to make sure it's alive, and attach info in data
			std::string answer =client->sendReceive(client->genRequest("PING", info));
		}

		client->closeSocket();
	} else {
		//failed to connect to primary
		if(tries > 0){
			std::cout<<"\n[RM] Failed to find primary\n";
			if(tries >3) {
				if(switchToPrimary()) {
					tries = 0;
				}
			}
		}
		//tried to reload data in case it was somehow changed
		loadPrimary();
		if(prim_ip == log_ptr->getIpThis() && prim_port == log_ptr->getPortThis()) {
			std::cout<<"\n\n[ERROR] I'm attempting to update primary info to self, aborting!\n\tThis is done to prevent a connection to self.\n\tBest to delete primary.txt at this point.\n\n";
		} else {
			client->changeConnection(prim_ip, prim_port);
		}
		tries++;

		backup_did_sync = false;
	}

}

//#########################################################################################################################
//##########									HELPERS
//=========================================================================================================================

bool ReplicationManager::integrityCheck() {

	if(log_ptr->getStatus() != "primary" && client->connectSocket()) {
		alive = true;
	}

	show_warning = false;
	std::cout<<"[RM] Connection detected, running integrity check:\n";
	std::vector<std::string> holder;
	log_ptr->getCommits(holder);
	while(!holder.empty()) {
		if(commit(holder.back())) {
			holder.pop_back();
		} else {
			printf("FAILED SYNC, in commit!\n");
			show_warning = true;
			if(log_ptr->getStatus() != "primary") {
				client->closeSocket();
				alive = false;
			}
			return false;
		}
	}
	printf("[RM] Integrity check complete. (SUCCESS) \n");
	if(log_ptr->getStatus() != "primary" && alive) {
		client->closeSocket();
		alive = false;
	}
	show_warning = true;
	return true;
}

//promote to primary
bool ReplicationManager::switchToPrimary() {

	if (log_ptr->getStatus() == "primary") {
		return true; //nothing to do
	} else {
		try {
			//write to primary.txt
		//----------------------------------------------------------
			std::string path = log_ptr->getNetDir();
			std::string filePath = path+"/primary.txt";
			// try to write in the location
			std::ofstream outfile (filePath.c_str());
			outfile << "IP: |"<<log_ptr->getIpThis()<<"|\tport: |"<<log_ptr->getPortThis()<<"|" << std::endl;
			outfile.close();

			if(outfile.fail() || outfile.bad())
			{
				perror("[Fail] writing to primary.txt: ");
				exit(EXIT_FAILURE);
			}
		//------------------------------------------------------------
			//write to log, and make self primary
			log_ptr->serverData(log_ptr->getIpThis(), log_ptr->getPortThis(), "primary", log_ptr->getNetDir());
			//client->closeSocket();
			//client->changeConnection("0.0.0.0", "0");



		} catch (...) {
			std::cout<<"Failed to make primary!!!!!!!!\n";
			return false;
		}
	}
	std::cout<<"\n===================================================\n [RM] Promoted self to Primary server.\n===================================================\n";
	return true;
}

bool ReplicationManager::switchToBackup() {
	if (log_ptr->getStatus() == "backup") {
		return true; //nothing to do
	} else {
		//write to log, and make self primary
		client->closeSocket();
		client->changeConnection("0.0.0.0","0");
		log_ptr->serverData(log_ptr->getIpThis(), log_ptr->getPortThis(), "backup", log_ptr->getNetDir());
		tries = -10;

		std::cout<<"\n===================================================\n [RM] Demoted self to Backup server.\n===================================================\n";
	}
	return true;
}

//read from primary.txt and load info
bool ReplicationManager::loadPrimary()
{
	bool ret = false;

	using namespace std;
	string path = log_ptr->getNetDir();
	string line;
	ifstream myfile(path+"/primary.txt");
	if (myfile.is_open())
	{
		getline(myfile, line);
		myfile.close();

	} else {
		std::cout<<"[ERROR] Failed to read primary.txt;\n * Please start primary first,\n * this one as main,\n * or make sure the path to primary.txt is correct.\n\nExiting..\n\n";
		exit(-1);
	}
	std::vector<std::string> strs;
	boost::split(strs, line, boost::is_any_of("|"));
	//std::cout<<"\n=======loaded from primary:=========\nIP-"<<strs[1]<<"\tport-"<<strs[3]<<"\n";
	prim_ip = strs[1];
	prim_port = strs [3];
	return ret;
}

ReplicationManager::~ReplicationManager()
{
	// TODO Auto-generated destructor stub
}

