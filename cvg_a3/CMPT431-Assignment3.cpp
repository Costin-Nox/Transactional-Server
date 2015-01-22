//============================================================================
// Name        	: CMPT431-Assignment3.cpp
// Author   	: Costin Ghiocel
// S/N			: 301027183
// Version     	:
// Copyright   	: Stealing is bad..mkay?
// Description 	: SFU-Comp-sci CMPT 431 Assignment 3
// Dependencies	: C++ lib 11; boost 15.xx+;
//============================================================================

#include "Server.h"
char* dir;
char* net_dir;
bool debug_rm = false;
bool printInfo = false;
bool debug_client = false;


void print_usage()
{
	printf("\nUsage:\n\t* server -[settings] +[options] \n\t Please see README.txt for info");
}

void exit_fn(int num) {

	puts("\n[Exiting] Removing log file.. \n");
	if( remove(".server.log") != 0 )
		perror( "Error deleting file" );
	else
		puts( "File successfully deleted" );

	puts("GoodBye\n\n");
	exit(0);
}

int main(int argc, char *argv[])
{

	signal(SIGINT, exit_fn);
	try
	{
		int opt = 0;
		std::string ip="NULL";
		short port = 8080;
		bool primary = false;


		static struct option long_options[] =
		{
		{ "ip", required_argument, 0, 'i' },
		{ "port", required_argument, 0, 'p' },
		{ "dir", required_argument, 0, 'd' },
		{ "master", no_argument, 0, 'm' },
		{ "network", required_argument, 0, 'n' },
		{ 0, 0, 0 } }
		;

		int long_index = 0;
		while ((opt = getopt_long(argc, argv, "i:p:d:n:m", long_options,
				&long_index)) != -1)
		{
			switch (opt)
			{
			case 'i':
				ip = optarg;
				break;
			case 'm':
				primary = true;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'd':
				dir = optarg;
				break;
			case 'n':
				net_dir = optarg;
				break;
			default:
				print_usage();
				exit(EXIT_FAILURE);
			}
		}

		// Optional arguments (mostly debug)
		for(int i = 3; i < argc; i++)
		{
			if (strcmp(argv[i], "+drm") == 0)	debug_rm = true;
			if (strcmp(argv[i], "+info") == 0)	printInfo = true;
			if (strcmp(argv[i], "+dc") == 0)	debug_client = true;
		}

		if (!dir || !net_dir)
		{
			puts( "A valid working directory and network directory for primary.txt are required." );
			print_usage();
			exit(EXIT_FAILURE);
		} else if(!boost::filesystem::is_directory(dir)) {
			std::cout<<"\n[!] ERROR: Directory not found: "<<dir<<std::endl;
			print_usage();
			exit(EXIT_FAILURE);
		} else if(!boost::filesystem::is_directory(net_dir)) {
			std::cout<<"\n[!] ERROR: Directory not found: "<<net_dir<<std::endl;
			print_usage();
			exit(EXIT_FAILURE);
		} else {
			chdir(dir);
			std::string filePath = "temp-testwrite-perm.tmp";
			// try to write in the location
			std::ofstream outfile (filePath.c_str());
			outfile << "I can write!" << std::endl;
			outfile.close();

			if(outfile.fail() || outfile.bad())
			{
				perror("[Fail] testing write access: ");
				exit(EXIT_FAILURE);
			} else {
				remove(filePath.c_str());
			}
		}

		if (!port)
		{
			port = 8080;
		}

		if (ip == "NULL")
		{
			ip = "127.0.0.1";
		}

		if(primary) {
			chdir(net_dir);
			std::string filePath = "primary.txt";
			// try to write in the location
			std::ofstream outfile (filePath.c_str());
			outfile << "IP: |"<<ip<<"|\tport: |"<<port<<"|" << std::endl;
			outfile.close();

			if(outfile.fail() || outfile.bad())
			{
				perror("[Fail] writing to primary.txt: ");
				exit(EXIT_FAILURE);
			}
		}

		boost::asio::io_service io_service;
		using namespace std;

		chdir(dir);

		string status;
		if(primary)
			status="primary";
		else
			status="backup";

		cout << "\n\nStarting Server in "<<status<<" mode...\n" << endl;
		Server s(io_service, ip, port, status, net_dir);
		cout << "Listening on ip: "<<ip<<" port: "<<port<<"\nWorking directory is:"<<dir<<"\nPrimary.txt location:"<<net_dir<<"\n"<<endl;
		cout<< "\t[!]Running as "<<status<<" server\n\n"<<endl;

		if(printInfo) {
		cout
		<< "[INFO]:\n"
		<< "=================================================================================\n"
		<< "* Illegal write request (type write and press enter in client)\n  produces a log display on both primary and backup\n"
		<< "* [TM] stands for transaction manager\n"
		<< "\t* transaction manager manages timeouts and so on, messages\n\t  will appear when something is deleted\n"
		<< "\t* expired transactions are still known for a while and server\n\t  will notify on them but ignore it\n"
		<< "* [RM] stands for replication Manager\n"
		<< "\t* [IMPORTANT NOTE]: please close backup using SIGKILL or similar,\n\t  SIGINT will cause it to delete log and that's bad.\n\t  Backup and Primary will be out of sync\n"
		<< "\t* if server is running as backup, it will resolve primary and send\n\t  the primary the ip and port for it (the backup)\n"
		<< "\t* the primary will wait for a backup to register, and attempt to\n\t  connect to it, it will be verbose about it\n"
		<< "\t* RM will display messages on syncs and the response it got\n"
		<< "\t* if the backup disconnects, it will notify and store a stack of\n\t  messages that need to be sent, which are synced when the backup registers\n"
		<< "\t* if there is no backup registered, it stores messages and sends\n\t  them when backup registers\n"
		<< "* Both servers behave as in assignment 2, you can continue a transaction\n  after a failure, keeps track of commits etc..\n"
		<< "=================================================================================="
		<< endl;
		}
		io_service.run();




	} catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}



	return 0;
}
