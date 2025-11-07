
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <limits>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cppcodec/base64_rfc4648.hpp>
#pragma GCC diagnostic pop

#include "helpers.h"

time_t duration2time(string t)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	strptime(("1970-01-01 " + t).c_str(), "%F %T", &tm);
	time_t ret = mktime(&tm) + 3600;

	return ret;
}

/* For allowed format strings: see
 * www.cplusplus.com/reference/iomanip/put_time
 */
int duration2sec(string t, string forceFormat)
{
	struct tm tm;
	memset(&tm, '\0', sizeof(struct tm));
	string t1 = trim(t);
	string t2 = "1970-01-01 " + t1;
	istringstream iss(t2);
	string format = "%Y-%m-%d ";
	if (forceFormat.empty()) {
		size_t len = t1.length();
		if (len == 8)
			format += "%H:%M:%S";
		else if (len == 5)
			format += "%M:%S";
		else if (len == 2)
			format += "%S";
		else {
			cout << "[" << __func__ << ":" << __LINE__ << "] " << "Error parse time string \""<< t1 << "\"\n" << endl;
			return 0;
		}
	}
	else
		format += forceFormat;
	iss >> get_time(&tm, format.c_str());
	if (iss.fail()) {
		cout << "[" << __func__ << ":" << __LINE__ << "] " << "Error parse time string \""<< t2 << "\", format: \""<< format <<"\"\n" << endl;
		return 0;
	}
	return static_cast<int>(mktime(&tm)) + 3600;
}

time_t str2time(string format, string t)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	strptime(t.c_str(), format.c_str(), &tm);
	time_t ret = mktime(&tm) + 3600;

	return ret;
}

/* For allowed format strings: see
 * www.cplusplus.com/reference/iomanip/put_time
 */
time_t str2time2(string format, string t)
{
	struct tm tm;
	memset(&tm, '\0', sizeof(struct tm));
	time_t tt = 0;
	istringstream iss(t);
	iss >> get_time(&tm, format.c_str());
	if (iss.fail()) {
		cout << "[" << __func__ << ":" << __LINE__ << "] " << "Error parse time string \""<< t << "\", format: \""<< format <<"\"\n" << endl;
		return 0;
	}
	tt = mktime(&tm);
	struct tm * ptm = gmtime(&tt);
	return mktime(ptm);
}

string trim(string &str, const string &trimChars /*= " \n\r\t"*/)
{
	string result = str.erase(str.find_last_not_of(trimChars) + 1);
	return result.erase(0, result.find_first_not_of(trimChars));
}

vector<string> split(const string &s, char delim)
{
	vector<string> vec;
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim))
		vec.push_back(item);
	return vec;
}

#if __cplusplus < 201103L
string to_string(int i)
{
	stringstream s;
	s << i;
	return s.str();
}

string to_string(unsigned int i)
{
	stringstream s;
	s << i;
	return s.str();
}

string to_string(long i)
{
	stringstream s;
	s << i;
	return s.str();
}

string to_string(unsigned long i)
{
	stringstream s;
	s << i;
	return s.str();
}

string to_string(long long i)
{
	stringstream s;
	s << i;
	return s.str();
}

string to_string(unsigned long long i)
{
	stringstream s;
	s << i;
	return s.str();
}
#endif

string& str_replace(const string &search, const string &replace, string &text)
{
	if (search.empty() || text.empty())
		return text;

	size_t searchLen = search.length();
	while (1) {
		size_t pos = text.find(search);
		if (pos == string::npos)
			break;
		text.replace(pos, searchLen, replace);
	}
	return text;
}

/*
 * ported from:
 * https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
 * 
 * You must delete the result if result is non-NULL
 */
const char *cstr_replace(const char *search, const char *replace, const char *text)
{
	const char *result;	// the return string
	const char *ins;	// the next insert point
	char *tmp;		// varies
	int len_search;		// length of search (the string to remove)
	int len_replace;	// length of replace (the string to replace search with)
	int len_front;		// distance between search and end of last search
	int count;		// number of replacements

	// sanity checks and initialization
	if (!text || !search)
		return NULL;
	len_search = strlen(search);
	if (len_search == 0)
		return NULL; // empty search causes infinite loop during count
	if (!replace)
		replace = "";
	len_replace = strlen(replace);

	// count the number of replacements needed
	ins = text;
	for (count = 0; (tmp = (char*)strstr(ins, search)); ++count)
		ins = tmp + len_search;

	int len_tmp = strlen(text) + (len_replace - len_search) * count + 1;
	tmp = new char[len_tmp];
	memset(tmp, '\0', len_tmp);
	result = (const char*)tmp;

	if (!result)
		return NULL;

	// first time through the loop, all the variable are set correctly
	// from here on,
	//    tmp points to the end of the result string
	//    ins points to the next occurrence of search in text
	//    text points to the remainder of text after "end of search"
	while (count--) {
		ins = strstr(text, search);
		len_front = ins - text;
		tmp = strncpy(tmp, text, len_front) + len_front;
		tmp = strncpy(tmp, replace, len_replace) + len_replace;
		text += len_front + len_search; // move to next "end of search"
	}
	strncpy(tmp, text, strlen(text));
	return result;
}

string str_tolower(string s)
{
	::transform(s.begin(), s.end(), s.begin(), static_cast<int(*)(int)>(::tolower));
	return s;
}

string str_toupper(string s)
{
	::transform(s.begin(), s.end(), s.begin(), static_cast<int(*)(int)>(::toupper));
	return s;
}

bool strEqual(const char* a, const char* b)
{
	if ((a == NULL) && (b == NULL))
		return true;

	string a_ = (a == NULL) ? "" : static_cast<string>(a);
	string b_ = (b == NULL) ? "" : static_cast<string>(b);
	return strEqual(a_, b_);
}

bool strEqual(string a, const char* b)
{
	string b_ = (b == NULL) ? "" : static_cast<string>(b);
	return strEqual(a, b_);
}

bool strEqual(const char* a, string b)
{
	string a_ = (a == NULL) ? "" : static_cast<string>(a);
	return strEqual(a_, b);
}

bool strEqual(string a, string b)
{
	if (a.empty() && b.empty())
		return true;

	if (a.empty() || b.empty())
		return false;

	return (a.compare(b) == 0);
}

string _getPathName(string &path, string sep)
{
	size_t pos = path.find_last_of(sep);
	if (pos == string::npos)
		return path;
	return path.substr(0, pos);
}

string _getBaseName(string &path, string sep)
{
	size_t pos = path.find_last_of(sep);
	if (pos == string::npos)
		return path;
	if (path.length() == pos +1)
		return "";
	return path.substr(pos+1);
}

string getPathName(string &path)
{
	return _getPathName(path, "/");
}

string getBaseName(string &path)
{
	return _getBaseName(path, "/");
}

string getFileName(string &file)
{
	return _getPathName(file, ".");
}

string getFileExt(string &file)
{
	return _getBaseName(file, ".");
}

string getRealPath(string &path)
{
	char buf[PATH_MAX];
	const char* ret = realpath(path.c_str(), buf);
	if (ret == NULL)
		return path;

	return (string)ret;
}


off_t file_size(const char *filename)
{
	struct stat stat_buf;
	if(::stat(filename, &stat_buf) == 0)
	{
		return stat_buf.st_size;
	} else
	{
		return 0;
	}
}

bool file_exists(const char *filename)
{
	struct stat stat_buf;
	if(::stat(filename, &stat_buf) == 0)
	{
		return true;
	} else
	{
		return false;
	}
}

string endlbr()
{
	stringstream ret;
	ret << (string)"<br />" << endl;
	return ret.str();
}

string readFile(string file)
{
	string ret_s;
	ifstream tmpData(file.c_str(), ifstream::binary);
	if (tmpData.is_open()) {
		tmpData.seekg(0, tmpData.end);
		int length = tmpData.tellg();
		tmpData.seekg(0, tmpData.beg);
		char* buffer = new char[length+1];
		tmpData.read(buffer, length);
		tmpData.close();
		buffer[length] = '\0';
		ret_s = (string)buffer;
		delete [] buffer;
	}
	else {
		cout << "Error read " << file << endl;
		return "";
	}

	return ret_s;
}

bool parseJsonFromFile(string& jFile, Json::Value *root, string *errMsg)
{
	string jData = readFile(jFile);
	bool ret = parseJsonFromString(jData, root, errMsg);
	jData.clear();
	return ret;
}

bool parseJsonFromString(string& jData, Json::Value *root, string *errMsg)
{
	Json::CharReaderBuilder builder;
	Json::CharReader* reader(builder.newCharReader());
	JSONCPP_STRING errs = "";
	const char* jData_c = jData.c_str();

	bool ret = reader->parse(jData_c, jData_c + strlen(jData_c), root, &errs);
	if (!ret || (!errs.empty())) {
		ret = false;
		if (errMsg != NULL)
			*errMsg = errs;
	}
	delete reader;
	return ret;
}

string writeJson2String(Json::Value json, string indent/*=""*/)
{
	Json::StreamWriterBuilder builder;
	builder["commentStyle"] = "None";
	builder["indentation"] = indent;
	Json::StreamWriter* writer(builder.newStreamWriter());

	stringstream ss;
	writer->write(json, &ss);
	string ret_s = ss.str();

	delete writer;
	return ret_s;
}

int safeStrToInt(string val)
{
	if (val.empty())
		return 0;

	string tmp_s = trim(val);
	int maxLen = (tmp_s.find("-") == 0) ? 11 : 10;
	tmp_s = tmp_s.substr(0, maxLen);
	long tmp_l = stol(tmp_s);

	tmp_l = min(tmp_l, static_cast<long>(numeric_limits<int>::max()));
	tmp_l = max(tmp_l, static_cast<long>(numeric_limits<int>::min()));
	return static_cast<int>(tmp_l);
}

string base64encode(const char* data, size_t len)
{
	return cppcodec::base64_rfc4648::encode(data, len);
}

string base64encode(string data)
{
	return base64encode(data.c_str(), data.length());
}

vector<unsigned char> base64decode_bin(string data)
{
	return cppcodec::base64_rfc4648::decode(data.c_str(), data.length());
}

string base64decode_str(string data)
{
	return cppcodec::base64_rfc4648::decode<string>(data.c_str(), data.length());
}

void resetStringstream(ostringstream* oss)
{
	oss->str(string());
	oss->clear();
}

void resetStringstream(istringstream* iss)
{
	iss->str(string());
	iss->clear();
}

void resetStringstream(stringstream* ss)
{
	ss->str(string());
	ss->clear();
}
