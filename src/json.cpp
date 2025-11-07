
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
#include <memory>
#include <string>

#include "common/helpers.h"
#include "json.h"
#include "sql.h"
#include "net.h"
#include "mt-api.h"

extern CMtApi*		g_mainInstance;
extern bool		g_debugMode;
extern int		g_apiMode;
extern int		g_queryMode;
extern string		g_msgBoxText;
extern string		g_jsonError;
extern string		g_dataRoot;

CJson::CJson()
{
	Init();
}

void CJson::Init()
{
	cooliSig1 = "Neutrino Mediathek";
	cooliSig2 = cooliSig1 + " - CST";
	cooliSig3 = cooliSig1 + "";
}

CJson::~CJson()
{
}

string CJson::styledJson(string json)
{
	string ret = json;
	Json::Value root;
	bool ok = parseJsonFromString(json, &root, NULL);
	if (ok)
		ret = root.toStyledString();

	return ret;
}

string CJson::styledJson(Json::Value json)
{
	return json.toStyledString();
}

void CJson::errorMsg(const char* func, int line, string msg)
{
	string resolved = msg.empty() ? "Error" : msg;
	string prefix = "[" + string(func) + ":" + std::to_string(line) + "]";
	g_msgBoxText = "<span style='color: OrangeRed'>" + prefix + " " + resolved + "</span>";

	g_jsonError = prefix + "\n" + resolved;
}

void CJson::parseError(const char* func, int line, string msg)
{
	string msg_ = (msg.empty()) ? "" :  + " (" + msg + ")";
	errorMsg(func, line, "Error parsing json data" + msg_);
}

void CJson::resetProgInfoStruct(progInfo_t* pi)
{
	pi->version	= "";
	pi->vdate	= 0;
	pi->mvversion	= "";
	pi->mvdate	= 0;
	pi->mventrys	= 0;
	pi->progname	= "";
	pi->progversion	= "";
	pi->api		= "";
	pi->apiversion	= "";
}

void CJson::resetLiveStreamStruct(livestreams_t* ls)
{
	ls->title	= "";
	ls->url		= "";
	ls->parse_m3u8	= 0;
}

void CJson::resetChannelStruct(channels_t* ch)
{
	ch->channel	= "";
	ch->count	= 0;
	ch->latest	= 0;
	ch->oldest	= 0;
}

void CJson::resetQueryHeaderStruct(query_header_t* qh)
{
	qh->software  = "";
	qh->vMajor    = 0;
	qh->vMinor    = 0;
	qh->isBeta    = 0;
	qh->vBeta     = false;
	qh->mode      = 0;
	qh->data.clear();
}

void CJson::resetCmdListVideoStruct(cmdListVideo_t* clv)
{
	clv->channel  = "";
	clv->timeMode = 0;
	clv->epoch    = 0;
	clv->duration = 0;
	clv->limit    = 0;
	clv->start    = 0;
	clv->refTime  = 0;
}

void CJson::resetListVideoStruct(listVideo_t* lv)
{
	lv->channel        = "";
	lv->theme          = "";
	lv->title          = "";
	lv->description    = "";
	lv->website        = "";
	lv->subtitle       = "";
	lv->url            = "";
	lv->url_small      = "";
	lv->url_hd         = "";
	lv->url_rtmp       = "";
	lv->url_rtmp_small = "";
	lv->url_rtmp_hd    = "";
	lv->url_history    = "";
	lv->date_unix      = 0;
	lv->duration       = 0;
	lv->size_mb        = 0;
	lv->geo            = "";
	lv->parse_m3u8     = 0;
}

void CJson::resetListVideoHeadStruct(listVideoHead_t* lvh)
{
	lvh->start   = 0;
	lvh->end     = 0;
	lvh->rows    = 0;
	lvh->total   = 0;
	lvh->refTime = 0;
}

bool CJson::asBool(Json::Value::iterator it)
{
	string tmp_s = it->asString();
	tmp_s = trim(tmp_s);
	return ((str_tolower(tmp_s) == "false") || (tmp_s == "0")) ? false : true;
}

bool CJson::parseListVideo(Json::Value root)
{
	cmdListVideo_t lv;
	resetCmdListVideoStruct(&lv);
	for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
		string name = it.name();
		if (name == "channel") {
			lv.channel = it->asString();
		}
		else if (name == "timeMode") {
				lv.timeMode = safeStrToInt(it->asString());
		}
		else if (name == "epoch") {
			lv.epoch = safeStrToInt(it->asString());
		}
		else if (name == "duration") {
			lv.duration = safeStrToInt(it->asString());
		}
		else if (name == "limit") {
			lv.limit = safeStrToInt(it->asString());
			}
		else if (name == "start") {
			lv.start = safeStrToInt(it->asString());
		}
		else if (name == "refTime") {
			lv.refTime = safeStrToInt(it->asString());
		}
	}

	g_mainInstance->csql->sqlListVideo(&lv, &listVideoHead, listVideo_v);

	return true;
}

bool CJson::parsePostData(string jData)
{
	string errMsg = "";
	Json::Value root;
	bool ok = parseJsonFromString(jData, &root, &errMsg);
	if (!ok) {
		parseError(__func__, __LINE__, errMsg);
		return false;
	}

	query_header_t qh;
	resetQueryHeaderStruct(&qh);
	if (root.isObject()) {
		for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
			string name = it.name();
			if (name == "software") {
				qh.software = it->asString();
			}
			else if (name == "vMajor") {
				qh.vMajor = safeStrToInt(it->asString());
			}
			else if (name == "vMinor") {
				qh.vMinor = safeStrToInt(it->asString());
			}
			else if (name == "isBeta") {
				qh.isBeta = asBool(it);
			}
			else if (name == "vBeta") {
				qh.vBeta = safeStrToInt(it->asString());
			}
			else if (name == "mode") {
				qh.mode = safeStrToInt(it->asString());
			}
			else if (name == "data") {
				qh.data = *it;
			}
		}
	}
	else {
		parseError(__func__, __LINE__);
		return false;
	}
	if (qh.data.isObject()) {
		g_queryMode = qh.mode;
		if (!strEqual(qh.software, cooliSig1) && !strEqual(qh.software, cooliSig2) && !strEqual(qh.software, cooliSig3)) {
#if 0
		string tmp_msg = "The given signature is '" + qh.software + "',\n"
			+ "but '" + cooliSig1 + "' or '" + cooliSig2 + "'\n"
			+ "or '" + cooliSig3 + "' is expected.";
#else
		string tmp_msg = "The given signature is '" + qh.software + "',\n"
			+ "but '" + cooliSig1 + "' or '" + cooliSig2 + "' is expected.";
#endif
			errorMsg(__func__, __LINE__, tmp_msg);
			return false;
		}
		if (qh.mode == queryMode_Info) {
			errorMsg(__func__, __LINE__, "Function not yet available.");
			return false;
		}
		else if (qh.mode == queryMode_listChannels) {
			errorMsg(__func__, __LINE__, "Function not yet available.");
			return false;
		}
		else if (qh.mode == queryMode_listLivestreams) {
			errorMsg(__func__, __LINE__, "Function not yet available.");
			return false;
		}
		else if (qh.mode == queryMode_listVideos) {
			return parseListVideo(qh.data);
		}
		else {
			errorMsg(__func__, __LINE__, "Unknown function.");
			return false;
		}
	}
	else {
		parseError(__func__, __LINE__);
		return false;
	}

	return true;
}

string CJson::liveStreamList2Json(vector<livestreams_t>& ls, string indent/*=""*/)
{
	Json::Value json;
	json["error"] = 0;

	Json::Value head;
	head["rows"] = ls.size();
	json["head"] = head;
	
	Json::Value entry(Json::arrayValue);
	for (size_t i = 0; i < ls.size(); i++) {
		Json::Value entryData;
		entryData["title"]	= trim(str_replace("Livestream", "", ls[i].title));
		entryData["url"]	= ls[i].url;
		entryData["parse_m3u8"]	= ls[i].parse_m3u8;
		entry.append(entryData);
	}
	ls.clear();
	json["entry"] = entry;

	return json2String(json, indent);
}

string CJson::channelList2Json(vector<channels_t>& ch, string indent/*=""*/)
{
	Json::Value json;
	json["error"] = 0;

	Json::Value head;
	head["rows"] = ch.size();
	json["head"] = head;
	
	Json::Value entry(Json::arrayValue);
	for (size_t i = 0; i < ch.size(); i++) {
		Json::Value entryData;
		entryData["channel"]	= ch[i].channel;
		entryData["count"]	= ch[i].count;
		entryData["latest"]	= ch[i].latest;
		entryData["oldest"]	= ch[i].oldest;
		entry.append(entryData);
	}
	ch.clear();
	json["entry"] = entry;

	return json2String(json, indent);
}

string CJson::videoList2Json(string indent/*=""*/)
{
	Json::Value json;
	json["error"] = 0;

	Json::Value head;
	head["start"]	= listVideoHead.start;
	head["end"]	= listVideoHead.end;
	head["rows"]	= listVideoHead.rows;
	head["total"]	= listVideoHead.total;
	head["refTime"]	= listVideoHead.refTime;
	json["head"]	= head;

	Json::Value entry(Json::arrayValue);
	for (size_t i = 0; i < listVideo_v.size(); i++) {
		Json::Value entryData;
		entryData["channel"]		= listVideo_v[i].channel;
		entryData["theme"]		= listVideo_v[i].theme;
		entryData["title"]		= listVideo_v[i].title;
		entryData["description"]	= listVideo_v[i].description;
		entryData["subtitle"]		= listVideo_v[i].subtitle;
		entryData["url"]		= listVideo_v[i].url;
		entryData["url_small"]		= listVideo_v[i].url_small;
		entryData["url_hd"]		= listVideo_v[i].url_hd;
		entryData["date_unix"]		= listVideo_v[i].date_unix;
		entryData["duration"]		= listVideo_v[i].duration;
		entryData["geo"]		= listVideo_v[i].geo;
		entryData["parse_m3u8"]		= listVideo_v[i].parse_m3u8;
#if 0
		entryData["url_rtmp"]		= listVideo_v[i].url_rtmp;
		entryData["url_rtmp_small"]	= listVideo_v[i].url_rtmp_small;
		entryData["url_rtmp_hd"]	= listVideo_v[i].url_rtmp_hd;
		entryData["url_history"]	= listVideo_v[i].url_history;
		entryData["website"]		= listVideo_v[i].website;
		entryData["size_mb"]		= listVideo_v[i].size_mb;
#endif
		entry.append(entryData);
	}
	listVideo_v.clear();
	json["entry"] = entry;

	return json2String(json, indent);
}

string CJson::progInfo2Json(progInfo_t* pi, string indent/*=""*/)
{
	Json::Value json;
	json["error"] = 0;

	Json::Value head(Json::arrayValue);
	Json::Value headData;
	json["head"] = head;

	Json::Value entry(Json::arrayValue);
	Json::Value entryData;
	entryData["version"]		= pi->version;
	entryData["vdate"]		= pi->vdate;
	entryData["mvversion"]		= pi->mvversion;
	entryData["mvdate"]		= pi->mvdate;
	entryData["mventrys"]		= pi->mventrys;
	entryData["progname"]		= pi->progname;
	entryData["progversion"]	= pi->progversion;
	entryData["api"]		= pi->api;
	entryData["apiversion"]		= pi->apiversion;
	entry.append(entryData);
	json["entry"] = entry;

	return json2String(json, indent);
}

string CJson::jsonErrMsg(string msg, int err/*=1*/)
{
	Json::Value json;
	Json::Value head(Json::arrayValue);
	Json::Value headData;
	json["head"] = head;

	json["error"] = err;
	json["entry"] = msg;

	return json2String(json);
}

string CJson::json2String(Json::Value json, string indent/*=""*/)
{
	return writeJson2String(json, indent);
}

string CJson::formatJson(string data, string tagBefore, string tagAfter)
{
	string html = readFile(g_dataRoot + "/template/json-format.html");
	data = base64encode(data);
	html = str_replace("@@@JSON_DATA@@@", data, html);

	return tagBefore + html + tagAfter;
}
