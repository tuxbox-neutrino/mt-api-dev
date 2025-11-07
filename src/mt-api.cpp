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

#define PROGVERSION "0.1.0"
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
bool			g_debugMode;
int			g_apiMode;
int			g_queryMode;
string			g_msgBoxText;
string			g_jsonError;

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
	if (queryString_mode.empty() || strEqual(queryString_mode, "index")) {
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
	g_dataRoot	= getPathName(g_documentRoot) + "/data";
	g_progName	= PROGNAME;
	g_progNameShort	= PROGNAMESHORT;
	g_progCopyright	= COPYRIGHT;
	g_progVersion	= "v" PROGVERSION;

	chtml		= new CHtml();
	cjson		= new CJson();
	csql		= new CSql();
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

	if (strEqual(queryString_mode, "api")) {
		queryString_submode = cnet->getGetValue(getData, "sub");
		if (strEqual(queryString_submode, "info")) {
			g_queryMode = queryMode_Info;
			if (!g_debugMode) {
				progInfo_t pi;
				cjson->resetProgInfoStruct(&pi);
				csql->sqlGetProgInfo(&pi);
				cout << cjson->progInfo2Json(&pi) << endl;
				return 0;
			}
		}
		else if (strEqual(queryString_submode, "listLivestream")) {
			g_queryMode = queryMode_listLivestreams;
			if (!g_debugMode) {
				vector<livestreams_t> ls;
				csql->sqlListLiveStreams(ls);
				cout << cjson->liveStreamList2Json(ls) << endl;
				return 0;
			}
		}
		else if (strEqual(queryString_submode, "listChannels")) {
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
