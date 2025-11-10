/*
	mt-api - Deliver json data based on a html request
	Copyright (C) 2017, M. Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef PROGVERSION
#define PROGVERSION "0.0.0"
#endif
#define PROGNAME "mediathek-api"
#define PROGNAMESHORT "mt-api"
#define COPYRIGHT "2015-2017© M. Liebmann (micha-bbg)"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <climits>
#include <ctime>

#include <jsoncpp/json/json.h>

#include "mt-api.h"
#include "net.h"
#include "html.h"
#include "json.h"
#include "sql.h"
#include "common/helpers.h"

CMtApi*			g_mainInstance;
const char*		g_progName;
const char*		g_progNameShort;
const char*		g_progVersion;
const char*		g_progCopyright;
string			g_documentRoot;
string			g_dataRoot;
string			g_logRoot;
bool			g_debugMode;
int			g_apiMode;
int			g_queryMode;
string			g_msgBoxText;
string			g_jsonError;

static string sanitizeForLog(string value, size_t maxLen = 512)
{
	if (value.empty())
		return "";

	for (size_t i = 0; i < value.size(); ++i) {
		if ((value[i] == '\n') || (value[i] == '\r') || (value[i] == '\t'))
			value[i] = ' ';
		else if (value[i] == '"')
			value[i] = '\'';
	}

	if ((maxLen > 0) && (value.size() > maxLen)) {
		value = value.substr(0, maxLen);
		value += "...(truncated)";
	}

	return value;
}

static string getRequestLogFilePath()
{
	static string logFilePath = "";

	if (!logFilePath.empty())
		return logFilePath;

	if (!g_logRoot.empty())
		logFilePath = g_logRoot + "/mt-api.requests.log";

	return logFilePath;
}

static void appendRequestLog(const string& message)
{
	string logFile = getRequestLogFilePath();
	if (logFile.empty())
		return;

	ofstream logStream(logFile.c_str(), ios::app);
	if (!logStream.good())
		return;

	time_t now = time(NULL);
	struct tm tmNow;
	char ts[32];
	string timestamp;
	if (localtime_r(&now, &tmNow) != NULL) {
		strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S%z", &tmNow);
		timestamp = ts;
	}
	else {
		timestamp = to_string(static_cast<long long>(now));
	}

	logStream << "[" << timestamp << "] " << message << endl;
}

static void logRequestStart(CNet* net, const string& mode)
{
	if (net == NULL)
		return;

	string method = sanitizeForLog(net->getEnv("REQUEST_METHOD"));
	string uri = sanitizeForLog(net->getEnv("REQUEST_URI"), 1024);
	string pathInfo = sanitizeForLog(net->getEnv("PATH_INFO"));
	string scriptName = sanitizeForLog(net->getEnv("SCRIPT_NAME"));
	string remoteAddr = sanitizeForLog(net->getEnv("REMOTE_ADDR"));
	string userAgent = sanitizeForLog(net->getEnv("HTTP_USER_AGENT"), 256);
	string host = sanitizeForLog(net->getEnv("HTTP_HOST"));
	string query = sanitizeForLog(net->getEnv("QUERY_STRING"));
	string modeVal = sanitizeForLog(mode);

	stringstream ss;
	ss << "event=request-start pid=" << getpid();
	if (!remoteAddr.empty())
		ss << " remote=\"" << remoteAddr << "\"";
	if (!host.empty())
		ss << " host=\"" << host << "\"";
	if (!method.empty())
		ss << " method=" << method;
	if (!uri.empty())
		ss << " uri=\"" << uri << "\"";
	if (!pathInfo.empty())
		ss << " path_info=\"" << pathInfo << "\"";
	if (!scriptName.empty())
		ss << " script=\"" << scriptName << "\"";
	if (!query.empty())
		ss << " query=\"" << query << "\"";
	if (!modeVal.empty())
		ss << " mode=\"" << modeVal << "\"";
	if (!userAgent.empty())
		ss << " ua=\"" << userAgent << "\"";

	appendRequestLog(ss.str());
}

static void logRequestTarget(CNet* net, const string& mode, const string& submode)
{
	if (net == NULL)
		return;

	string modeVal = sanitizeForLog(mode);
	string subVal = sanitizeForLog(submode);
	if (modeVal.empty() && subVal.empty())
		return;

	string remoteAddr = sanitizeForLog(net->getEnv("REMOTE_ADDR"));

	stringstream ss;
	ss << "event=request-target pid=" << getpid();
	if (!remoteAddr.empty())
		ss << " remote=\"" << remoteAddr << "\"";
	if (!modeVal.empty())
		ss << " mode=\"" << modeVal << "\"";
	if (!subVal.empty())
		ss << " sub=\"" << subVal << "\"";

	appendRequestLog(ss.str());
}

static void logRequestPayload(CNet* net, const string& mode, const string& submode, const string& payload)
{
	if ((net == NULL) || payload.empty())
		return;

	string remoteAddr = sanitizeForLog(net->getEnv("REMOTE_ADDR"));
	string modeVal = sanitizeForLog(mode);
	string subVal = sanitizeForLog(submode);
	string payloadVal = sanitizeForLog(payload, 4096);

	stringstream ss;
	ss << "event=request-payload pid=" << getpid();
	if (!remoteAddr.empty())
		ss << " remote=\"" << remoteAddr << "\"";
	if (!modeVal.empty())
		ss << " mode=\"" << modeVal << "\"";
	if (!subVal.empty())
		ss << " sub=\"" << subVal << "\"";
	ss << " data1=\"" << payloadVal << "\"";

	appendRequestLog(ss.str());
}

void myExit(int val);

CMtApi::CMtApi()
{
	cnet		= NULL;
	chtml		= NULL;
	cjson		= NULL;
	csql		= NULL;
	g_debugMode	= false;
	g_apiMode	= apiMode_unknown;
	g_queryMode	= queryMode_None;
	g_msgBoxText	= "";
	indexMode	= false;
	Init();
}

string CMtApi::addTextMsgBox(bool clear/*=false*/)
{
	g_msgBoxText = base64encode(g_msgBoxText);
	string html = readFile(g_dataRoot + "/template/msgbox.html");
	html = str_replace("@@@MSGTXT@@@", (clear)?"":g_msgBoxText, html);
	g_msgBoxText = "";
	return html;
}

void CMtApi::Init()
{
	/* read GET data */
	string inData;
	cnet = new CNet();
	cnet->readGetData(inData);
	cnet->splitGetInput(inData, getData);
	queryString_mode = cnet->getGetValue(getData, "mode");
	const string modeLowerInit = str_tolower(queryString_mode);
	if (modeLowerInit.empty() || strEqual(modeLowerInit, "index")) {
		indexMode = true;
	}
	string tmp_s = cnet->getEnv("SERVER_NAME");
	g_debugMode = ((tmp_s.find(".debug.coolithek.") != string::npos) ||
		       (tmp_s.find("coolithek.slknet.de") == 0) ||
		       (tmp_s.find(".deb.") != string::npos) ||
		       (tmp_s.find("neutrino-mediathek.de") == 0) ||
		       (tmp_s.find("www.neutrino-mediathek.de") == 0) ||
		       (indexMode == true));
	string cth = (g_debugMode) ? "text/html; charset=utf-8" : "application/json; charset=utf-8";
	cnet->sendContentTypeHeader(cth);
//#ifdef SANITIZER
	if (g_debugMode) {
		dup2(STDOUT_FILENO, STDERR_FILENO);
	}
//#endif

	g_documentRoot  = cnet->getEnv("DOCUMENT_ROOT");
	string installRoot = getPathName(g_documentRoot);
	g_dataRoot	= installRoot + "/data";
	g_logRoot	= installRoot + "/log";
	g_progName	= PROGNAME;
	g_progNameShort	= PROGNAMESHORT;
	g_progCopyright	= COPYRIGHT;
	g_progVersion	= "v" PROGVERSION;

	chtml		= new CHtml();
	cjson		= new CJson();
	csql		= new CSql();

	logRequestStart(cnet, queryString_mode);
}

CMtApi::~CMtApi()
{
	if (cnet != NULL)
		delete cnet;
	if (chtml != NULL)
		delete chtml;
	if (cjson != NULL)
		delete cjson;
	if (csql != NULL)
		delete csql;
}

int CMtApi::run(int, char**)
{
	if (indexMode) {
		htmlOut << chtml->getIndexSite();
		cout << chtml->tidyRepair(htmlOut.str(), 0) << endl;
		return 0;
	}

	csql->connectMysql();

	const string modeLower = str_tolower(queryString_mode);
	if (strEqual(modeLower, "api")) {
		queryString_submode = cnet->getGetValue(getData, "sub");
		const string subLower = str_tolower(queryString_submode);
		logRequestTarget(cnet, queryString_mode, queryString_submode);
		if (strEqual(subLower, "info")) {
			g_queryMode = queryMode_Info;
			if (!g_debugMode) {
				progInfo_t pi;
				cjson->resetProgInfoStruct(&pi);
				csql->sqlGetProgInfo(&pi);
				cout << cjson->progInfo2Json(&pi) << endl;
				return 0;
			}
		}
		else if (strEqual(subLower, "listlivestream")) {
			g_queryMode = queryMode_listLivestreams;
			if (!g_debugMode) {
				vector<livestreams_t> ls;
				csql->sqlListLiveStreams(ls);
				cout << cjson->liveStreamList2Json(ls) << endl;
				return 0;
			}
		}
		else if (strEqual(subLower, "listchannels")) {
			g_queryMode = queryMode_listChannels;
			if (!g_debugMode) {
				vector<channels_t> ch;
				csql->sqlListChannels(ch);
				cout << cjson->channelList2Json(ch) << endl;
				return 0;
			}
		}
		else {
			/* read POST data */
			string inData;
			cnet->readPostData(inData);
			if (!inData.empty()) {
				cnet->splitPostInput(inData, postData);
				if (!postData.empty())
					inJsonData = cnet->getPostValue(postData, "data1");
				if (!postData.empty() && !inJsonData.empty())
					logRequestPayload(cnet, queryString_mode, queryString_submode, inJsonData);
			}
			if (inJsonData.empty())
				inJsonData = readFile(g_dataRoot + "/template/test_1.json");

			if (!g_debugMode) {
				bool parseIO = cjson->parsePostData(inJsonData);
				if (parseIO) {
					if (g_queryMode == queryMode_listVideos) {
						cout << cjson->videoList2Json() << endl;
					}
				}
				else {
					string msg = (g_jsonError.empty()) ? "API Error" : g_jsonError;
					cout << cjson->jsonErrMsg(msg) << endl;
				}

				return 0;
			}
		}
	}
	else if ((queryString_mode.find("page") == 3) && (queryString_mode.length() == 7)) {
		/* 000page */
		htmlOut << chtml->getErrorSite(atoi(queryString_mode.c_str()), "");
		cout << chtml->tidyRepair(htmlOut.str(), 0) << endl;
		return 0;
	}
	else {
		htmlOut << chtml->getErrorSite(404, queryString_mode);
		cout << chtml->tidyRepair(htmlOut.str(), 0) << endl;
		return 0;
	}

	if (g_queryMode == queryMode_None)
		g_queryMode = queryMode_beginPOSTmode;

	if (g_debugMode) {
		int headerFlags = 0;
		headerFlags |= CHtml::includeCopyR;
		headerFlags |= CHtml::includeGenerator;
		headerFlags |= CHtml::includeApplication;
		htmlOut << chtml->getHtmlHeader("Coolithek API", headerFlags);

		string mainBody = readFile(g_dataRoot + "/template/main-body.html");
		inJsonData = cjson->styledJson(inJsonData);
		if (g_queryMode < queryMode_beginPOSTmode)
			mainBody = str_replace("@@@JSON_TEXTAREA@@@", "{}", mainBody);
		else
			mainBody = str_replace("@@@JSON_TEXTAREA@@@", inJsonData, mainBody);
		htmlOut << mainBody;

		if (g_queryMode == queryMode_Info) {
			progInfo_t pi;
			cjson->resetProgInfoStruct(&pi);
			csql->sqlGetProgInfo(&pi);
			string tmp_json = cjson->progInfo2Json(&pi, "  ");
			tmp_json = cnet->decodeData(tmp_json);
			htmlOut << cjson->formatJson(tmp_json) << endl;
		}
		else if (g_queryMode == queryMode_listLivestreams) {
			vector<livestreams_t> ls;
			csql->sqlListLiveStreams(ls);
			string tmp_json = cjson->liveStreamList2Json(ls, "  ");
			tmp_json = cnet->decodeData(tmp_json);
			htmlOut << cjson->formatJson(tmp_json) << endl;
		}
		else if (g_queryMode == queryMode_listChannels) {
			vector<channels_t> ch;
			csql->sqlListChannels(ch);
			string tmp_json = cjson->channelList2Json(ch, "  ");
			tmp_json = cnet->decodeData(tmp_json);
			htmlOut << cjson->formatJson(tmp_json) << endl;
		}
		else if (g_queryMode >= queryMode_beginPOSTmode) {
			bool parseIO = cjson->parsePostData(inJsonData);
			if (parseIO) {
				if (g_queryMode == queryMode_listVideos) {
					string tmp_json = cjson->videoList2Json("  ");
					tmp_json = cnet->decodeData(tmp_json);
					htmlOut << cjson->formatJson(tmp_json) << endl;
				}
			}
		}

		if (!g_msgBoxText.empty())
			htmlOut << addTextMsgBox();

		htmlOut << chtml->getHtmlFooter(g_dataRoot + "/template/footer.html", "<hr style='width: 80%;'>") << endl;

		/* Output data repaired by tidy */
		cout << chtml->tidyRepair(htmlOut.str(), 0) << endl;
	}
	else {
		string json = "{ \"error\": 1, \"head\": [], \"entry\": \"Unsupported parameter.\" }";
//		json = cjson->styledJson(json);
		json = cjson->styledJson(inJsonData);
		cout << json << endl;
	}

	return 0;
}

void myExit(int val)
{
	exit(val);
}

int main(int argc, char *argv[])
{
	g_mainInstance = NULL;

	/* main prog */
	g_mainInstance = new CMtApi();
	int ret = g_mainInstance->run(argc, argv);
	delete g_mainInstance;

	return ret;
}
