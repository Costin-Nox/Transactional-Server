/*
 * Session.cpp
 *
 *  Created on: Oct 20, 2014
 *      Author: costin
 *
 *     This contains the bulk of the server logic.
 */

#include "Session.h"

//builder take in io_service as per boost spec// extra args:log address
Session::Session(boost::asio::io_service& io_service, LogHandler &server_log, ReplicationManager &rm) :
		socket_(io_service),io_service_(io_service)
{
	// Frowned upon, but the beauty of C++, creating a new local object with the memory address of the passed reference.
	// Essentially accessing the persistent object called by the parent.
	log_ptr = &server_log;
	rep_man = &rm;
	auth_port = 0;
	auth_ip ="0.0.0.0";
}

//called on connection timeout~
Session::~Session()
{

}

//-- HELPER functions: ----------------
//-------------------------------------

bool Session::sendACK(std::string s)
{
	write(	socket_, 								//send to
			boost::asio::buffer(s, s.length())		//send what
						);
	return true;
}

tcp::socket& Session::socket()
{
	return socket_;
}

void Session::sendACK_RESEND(int tsn) {
	std::vector<int> stack;
	std::string s;
	log_ptr->canCommit(tsn, log_ptr->getStatusData(tsn), stack);
	while (!stack.empty()) {
		s = "empty\t" + iToS(tsn) + "\t" + iToS(stack.back()) + "\t";
		sendACK(genAnswerStr("ACK_RESEND", s, "0", " "));
		stack.pop_back();
	}
}

/*
 * this function is used to help read the payload (data), it takes in the size reported in the header
 * */
std::size_t Session::completion_condition(
		// Result of latest read_some operation.
		const boost::system::error_code& error,

		// Number of bytes transferred so far.
		std::size_t bytes_transferred,

		//size of payload
		int total)
//Start method--------------------->
{

	if (!error)
	{
		std::size_t ret = total - bytes_transferred;
		if (ret > 0)
			return ret;
		else
			return 0;
	}
	else
	{
		return 0;
	}
}

bool Session::executeCommand(int i, std::string header, std::string data,
		std::string &error)
{

	std::string sClientIp = socket().remote_endpoint().address().to_string();
	unsigned short uiClientPort = socket().remote_endpoint().port();
	if(i != 8 && !isAuth(sClientIp, uiClientPort) && log_ptr->getStatus() != "primary") {
		std::cout<<"[ERROR] Failed authorization\n";
		error = genAnswerStr("AUTH_FAIL", header, "321", "Not authorized");
		return false;
	}

	/*
	 * 	ABORT = 1;
	 NEW_TXN = 2;
	 COMMIT = 3;
	 READ = 4;
	 WRITE = 5;
	 MAKE_TXN = 6
	 PING = 7
	 AUTH = 8;
	 *
	 * rep_man is the replication manager, successful packets are enqueued to be sent
	 * 	- we went to only sned successful ones, otherwise things like expired transactions
	 * 	which refuse it here, might not refuse it on the back-up, there are no guarantees thatexpirations and so on are synced.
	 *
	 */

	if (i < 1)
	{
		error = genAnswerStr("ERROR", header, "204", "Invalid method");
		//std::cout<<"inv method...\n";
		return false;
	}

	std::string s_read;
	bool ret;
	switch (i)
	{
	case 1: //ABORT
		ret = log_ptr->abort(header, error);
		if(ret) {
			sendACK(genAnswerStr("ACK", header, "0", "Aborted"));
			rep_man->enqueue(header,data);
		}
		return ret;
	case 2: //NEW_TXN
		int tsn_id;
		tsn_id = log_ptr->newTxn(data);
		if(tsn_id == 0) {//invalid name
			error = genAnswerStr("ERROR", header, "205", "Invalid file name.");
			return false;
		}
		sendACK("ACK\t" + boost::lexical_cast<std::string>(tsn_id) + "\t0" + "\t0" +"\t0"+"\r\n\r\n\r\n");
		s_read = "MAKE_TXN "+boost::lexical_cast<std::string>(tsn_id)+" 0 0 0";
		rep_man->enqueue(s_read, data);
		return true;
	case 3: //COMMIT
		ret = log_ptr->commit(header, error);
		if(ret) {
			rep_man->enqueue(header,data);
		}
		return ret;
	case 4: //READ

		if(log_ptr->readFile(data, s_read)) {
			sendACK(genAnswerStr("ACK", header, "0", s_read));
			return true;
		} else {
			error = genAnswerStr("ERROR", header, "206", "File not found");
			return false;
		}
		return false;

	case 5: //WRITE
		 ret = log_ptr->addLogEntry(header, data, error);
		 if (ret) {
			 sendACK(genAnswerStr("ACK", header, "0", " "));
			 rep_man->enqueue(header,data);
		 } else if(error == "committed") {
			 rep_man->enqueue(header,data);
		 }

		 return ret;
	case 6: //MAKE_TXN
		log_ptr->makeTxn(data, getTsn(header));
		sendACK(genAnswerStr("ACK", header, "0", "Created"));
		return true;
	case 7: //PING
		sendACK(genAnswerStr("ACK", header, "200", " "));
		if(!log_ptr->backupRegistered()) {
			log_ptr->backupServerData(data);
		}
		return true;
	case 8: //AUTH
		doAuth(sClientIp,uiClientPort);
		return true;
	case 9: //KILL
		rep_man->switchToBackup();
		return true;
	default:
		error = genAnswerStr("ERROR", header, "204", "Invalid method");
		return false;

	}
	error = genAnswerStr("ERROR", header, "204", "Invalid method");
	return false;
}

//initialize
void Session::start()
{
	try
	{
		async_read_until(socket_, b, "\r\n\r\n", //from, to, delimiter
				//handler + binds
				boost::bind(&Session::handle_read, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
	catch (boost::exception &e)
	{
		// data was incorrect and it reached end of stream without being able to find delimiter
		std::cout<<"ERROR: Incorrect header format (could not find delimiter most likely)\nPacket ignored, continuing..\n";
	}


}


/*
 * callback for asyn_read: this will be called only when it finds the header delimiter
 * 	--------------------------------------------------------------------------------------
 */
void Session::handle_read(const boost::system::error_code& error,
		size_t bytes_transferred)
{

	if (!error)
	{

		//parse stream fetched from the socket
		std::istream is(&b);
		//define some empty holders
		std::string header, fragment, data_stream;
		//get the header
		std::getline(is, header);
		header.erase( std::remove(header.begin(), header.end(), '\r'), header.end() );


		while(header.size() < 8) { //header packet has to be at least 8 bytes, otherwise it's garbage.{a 0 0 0}
			if(std::getline(is, header)) {
				header.erase( std::remove(header.begin(), header.end(), '\r'), header.end() );
			} else {
				sendACK(genAnswerStr("ERROR", "a -2 0 0", "204", "Issue parsing packet [case: end of stream]"));
				start();
				return;
			}
		}
		if(!validHeader(header)) {
			sendACK(genAnswerStr("ERROR", "a -1 0 0", "204", "Header is invalid or orphaned data (invalid op with data)"));
			start();
			return;
		}
		//if there was data, there will be one blank line, need to remove this (no data will be 2, but that's for the next async iter to remove)
		std::getline(is, fragment);
		/* the way async read works is that it will grab chunks of random size from the socket repeatedly until *some* condition.
		 * in my case, until it finds the delimiter. The issue is, it sometimes reads past that, so you end up with the case: [a, b, c *delimiter* fragment]]
		 * without capturing the fragment it would be lost as the data is removed from the buffer. The fragment might be part of the data if it exists */
		std::getline(is, fragment);
		//if there was a fragment, we need to compute the size so it can be subtracted from the size reported in the header
		int pre = fragment.size();

		//was there data associated the the packet?
		//---------------------------------------------------------------------------------------------------------------------------
		if (dataAttached(header))
		{

			try
			{
				 /*
				  * the completion condition gets checked AFTER it reads SOME bytes
				  * this means that if there is nothing to read, it will just wait for something util
				  * the socket times out, which is no good, so if the fragment is the size of the expected data,
				  * there is nothing left to read, so ignore this.
				  *
				  * Pain in the ass to test, since i can't make it happen, it's a really stupid bug, hoping this will fix it..
				  */
				if(getBytes(header) != pre+1) {

					read(
							//from, to
							socket_, c,
							//completion condition [in this case, read for the number of bytes reported in the header]
							boost::bind(&Session::completion_condition, this,
									//bind extras to the function
									boost::asio::placeholders::error,
									boost::asio::placeholders::bytes_transferred,
									getBytes(header) - pre));

					//define new istream for data
					std::istream cs(&c);
					std::getline(cs, data_stream);
					//combine fragment and data
					data_stream = fragment + data_stream;
				} else {//all the data was in the fragment
					data_stream = fragment;
				}

			} catch (boost::exception &e)
			{
				//size data was incorrect and it reached end of stream without being able to stop by fn condition;
				//the client includes the escape
				std::cout
						<< "*********************\nError reading data field, size correct?\nPacket ignored.. continuing to listen\n\n"
						<<" This error should only happen if data size is incorrect, the function gets stuck waiting for data that never arrives,"
						<<" this goes on until the socket times out and it returns an error.. which is what triggers this message.\n"
						<<"\n*********************"
						<< std::endl;
				//return void to stop execution chain and avoid errors from trying to process partial info
				return;
			}

			/*
			 * ACT on DATA
			 * --------------------------------------------------------------------------------------------------------------------------
			 */
			//async
			start();

			//create a string to be passed for errors (or other info)
			std::string error_execute;
			//act on data received
			bool ex = executeCommand(parse(header), header, data_stream,
					error_execute);
			//error?
			if (!ex) {
				if(error_execute == "incomplete") {
					sendACK_RESEND(getTsn(header));
				} else if(error_execute == "committed") {
					doCommit(header);
				} else {
					sendACK(error_execute);
				}
			}

		}
		//no data attached
		else
		{


			if(needsData(header)) {
				sendACK(genAnswerStr("ERROR", header, "204", "Method requires data."));
				start();
				return;
			}
			//async
			start();

			//create a string to be passed for errors (or other info)
			std::string error_execute;
			//act on data received
			bool ex = executeCommand(parse(header), header, " ",
					error_execute);
			//error?
			if (!ex) {
				if(error_execute == "incomplete") {
					sendACK_RESEND(getTsn(header));

				}
				//commit requested
				else if(error_execute == "committed") {
					doCommit(header);
				} else {
					sendACK(error_execute);
				}
			}
		}

	}
	else
	{
		//std::cout<<"del\n";
		delete this;
	}
}

void Session::doCommit(std::string header) {
	//generate message
	std::string ack_msg = genAnswerStr("ACK", header, "0", "Committed successfully.");
	//check if this is primary
	if(log_ptr->getStatus() == "primary") {

		//Generate string to fake a commit message to enqueue for replication
		int seq = log_ptr->getStatusData(getTsn(header));
		std::string commit_message_gen;
		if(seq == 0) {
			commit_message_gen = "COMMIT "+iToS(getTsn(header))+" "+iToS(getSeq(header))+" 0";
		} else {
			commit_message_gen = "COMMIT "+iToS(getTsn(header))+" "+iToS(seq)+" 0";
		}
		if(rep_man->commit(commit_message_gen)) { //wait for backup to commit
			//perform commit here-->
			int tsn = getTsn(header);
			if(log_ptr->writeFile(tsn)) {
				log_ptr->changeStatus(tsn, "COMMITTED");
			} else {
				std::cout<<"[CRITICAL ERROR] Backup committed, but I failed to do so, inconsistent state, please fix manually.\n\nEXITING\n\n";
				exit(-1);
			}
			log_ptr->save();

			//end commit ----------->

			sendACK(ack_msg);
		} else { //backup failed to commit
			ack_msg = genAnswerStr("ERROR", header, "205", "commit fail, i/o error");
			sendACK(ack_msg);
		}
	} else { //backup server, so just ACK the message
		sendACK(ack_msg);
	}

}
