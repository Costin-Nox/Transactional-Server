/*
 * ServerTools.cpp
 *
 *  Created on: Oct 21, 2014
 *      Author: costin
 */

#include "ServerTools.h"

//random seed
boost::mt19937 gen(time(0));

ServerTools::ServerTools()
{

	// ugh... -_- strings don't convert to ints; no mapping no switch statements, so.. there's that
	map["ABORT"] = 1;
	map["NEW_TXN"] = 2;
	map["COMMIT"] = 3;
	map["READ"] = 4;
	map["WRITE"] = 5;
	map["MAKE_TXN"] = 6;
	map["PING"] = 7;
	map["AUTH"] = 8;
	map["KILL"] = 9;

}

ServerTools::~ServerTools()
{
	// TODO Auto-generated destructor stub
}

int ServerTools::parse(std::string s)
{
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));
	std::string method = strs[0];

	if (map.find(method) == map.end())
		return 0;
	else
		return map[method];
}

bool ServerTools::validHeader(std::string s) {
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));

	if(strs.size()!=4) {
		std::cout<<"Size is: "<< strs.size()<<" | string: "<<s<<"\n";
		return false;
	}

	return true;
}

bool ServerTools::needsData(std::string s) {
	std::string s1 = s;
	int method = parse(s);

	switch (method)
	{

	case 2: //NEW_TXN
		return true;
	case 4: //READ
		return true;
	case 5: //WRITE
		return true;
	case 6: //MAKE_TXN
		return true;
	case 7: //ping
		return true;

	}

	return false;
}

bool ServerTools::dataAttached(std::string s)
{
	std::string s1 = s;
	int method = parse(s);
	int length = getBytes(s1);

	switch (method)
	{

	case 2: //NEW_TXN
		return (length > 0);
	case 4: //READ
		return (length > 0);
	case 5: //WRITE
		return (length > 0);
	case 6: //MAKE_TXN
		return (length > 0);
	case 7: //PING
		return (length > 0);

	}

	return false;
}

bool ServerTools::checkPath(std::string path_addr, std::string &result_string)
{

	using namespace boost::filesystem;

	path p(path_addr);

	if (exists(p))    // does p actually exist?
	{
		if (is_regular_file(p))        // is p a regular file?
			result_string = "file";

		else if (is_directory(p))      // is p a directory?
			result_string = "dir";

		else {
			result_string = "Error: found something, but can't do anything with it; permissions correct?";
			return false;
		}
	}
	else {
		result_string = "Check path, nothing there.";
		return false;
	}

	return true;
}

// generate a proper string for the ACK
std::string ServerTools::genAnswerStr(std::string method, std::string header,
		std::string code, std::string err_string)
{

	std::string toRet;
	std::vector<std::string> strs;
	boost::split(strs, header, boost::is_any_of("\t "));
	if(strs.size() <3) { //prevent potential seg faults
		strs[2] = '0';
		if(strs.size() <2)
			strs[1] = '0';
	}
	int size_err = err_string.size();

	toRet = method + " " + strs[1] + " " + strs[2] + " " + code + " ";
	if (err_string == " ")
		toRet = toRet + "0" + "\r\n\r\n\r\n";
	else
		toRet = toRet + boost::lexical_cast<std::string>(size_err) + "\r\n\r\n"
				+ err_string + "\r\n";

	return toRet;
}

// i could make these all one function, but this class is inherited in a few places and this keeps the code way more readable.
// names should be descriptive enough, no comments.

int ServerTools::getBytes(std::string s)
{
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));

	//check num of args
	if(strs.size()<4) {

		return -100;
	}

	return atoi(strs[3].c_str());

}

int ServerTools::getBytesResponse(std::string s)
{
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));

	//check num of args
	if(strs.size()<5) {

		return -100;
	}

	return atoi(strs[4].c_str());

}

int ServerTools::getSeq(std::string s)
{
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));

	//check num of args
	if(strs.size()<3)
		return -100;
	//attempt to cast to int
	using boost::lexical_cast;
	using boost::bad_lexical_cast;
	int ret;
	try
	{
		ret = lexical_cast<int>(strs[2]);
	}
	catch(const bad_lexical_cast &)
	{
		ret = -100;
	}

	return ret;

}

int ServerTools::getTsn(std::string s)
{
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));

	//check num of args
	if(strs.size()<2)
		return -100;
	//attempt to cast to int
	using boost::lexical_cast;
	using boost::bad_lexical_cast;
	int ret;
	try
	{
		ret = lexical_cast<int>(strs[1]);
	}
	catch(const bad_lexical_cast &)
	{
		ret = -100;
	}

	return ret;

}

std::string ServerTools::getCommand(std::string s)
{
	std::vector<std::string> strs;
	boost::split(strs, s, boost::is_any_of("\t "));

	if(strs.empty())
		return "NULL";

	return strs[0];

}

int ServerTools::randomNum(int upto)
{
	const int min = 1;
	const int max = upto;
	boost::uniform_int<> dist(min, max);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<> > die(gen,dist);
	return die();
}

