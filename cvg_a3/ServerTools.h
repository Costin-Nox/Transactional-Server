/*
 * ServerTools.h
 *
 *  Created on: Oct 21, 2014
 *      Author: Costin Ghiocel
 *
 *  Utilities file, inherited in almost every class, lots of includes, in alphabetic order-ish
 */

#ifndef ServerTools_H_
#define ServerTools_H_
//STD Lib includes
#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <getopt.h>
#include <signal.h>
#include <cstdlib>
//BOOST LIB INCLUDES
#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#define BOOST_NO_CXX11_SCOPED_ENUMS //fix a linking issue
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/map.hpp>
#include <boost/thread.hpp>



//inherited by session to perform operations on data
class ServerTools
{

public:
	ServerTools();
	virtual ~ServerTools();
	int parse(std::string s);
	bool parseAndDo(std::string s);
	bool dataAttached(std::string s);
	bool checkPath(std::string path_addr, std::string &result_string);
	int getBytes(std::string s);
	int getBytesResponse(std::string s);
	int getTsn(std::string s);
	int getSeq(std::string s);
	int randomNum(int upto);
	bool validHeader(std::string s);
	bool needsData(std::string s);

	std::string genAnswerStr(std::string method, std::string header, std::string code, std::string err_string);
	std::string getCommand(std::string s);
	std::string iToS(int i) {
		return boost::lexical_cast<std::string>(i);
	}


private:
	std::map<std::string, int> map;
	bool write_file(int txn, int seq, std::string data);
	bool read_file(int txn, int seq, int length, std::string filename);

};



#endif /* ServerTools_H_ */
