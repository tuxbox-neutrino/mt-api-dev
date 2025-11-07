
#ifndef __helpers_h__
#define __helpers_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include <string>
#include <vector>
#include <iostream>
#include <cctype>
#include <clocale>
#include <algorithm>
#include <jsoncpp/json/json.h>

using namespace std;

time_t duration2time(string t);
int duration2sec(string t, string forceFormat = "");
time_t str2time(string format, string t);
time_t str2time2(string format, string t);

string trim(string &str, const string &trimChars = " \n\r\t");

vector<string> split(const string &s, char delim);

#if __cplusplus < 201103L
string to_string(int);
string to_string(unsigned int);
string to_string(long);
string to_string(unsigned long);
string to_string(long long);
string to_string(unsigned long long);
#endif
string& str_replace(const string &search, const string &replace, string &text);
const char *cstr_replace(const char *search, const char *replace, const char *text);

string str_tolower(string s);
string str_toupper(string s);

bool strEqual(const char* a, const char* b);
bool strEqual(string a, const char* b);
bool strEqual(const char* a, string b);
bool strEqual(string a, string b);

string getPathName(string &path);
string getBaseName(string &path);
string getFileName(string &file);
string getFileExt(string &file);
string getRealPath(string &path);

off_t file_size(const char *filename);
bool file_exists(const char *filename);

string endlbr();
string readFile(string file);
bool parseJsonFromFile(string& jFile, Json::Value *root, string *errMsg);
bool parseJsonFromString(string& jData, Json::Value *root, string *errMsg);
string writeJson2String(Json::Value json, string indent="");
int safeStrToInt(string val);

string base64encode(const char* data, size_t len);
string base64encode(string data);
vector<unsigned char> base64decode_bin(string data);
string base64decode_str(string data);

void resetStringstream(ostringstream* oss);
void resetStringstream(istringstream* iss);
void resetStringstream(stringstream* ss);

#endif // __helpers_h__
