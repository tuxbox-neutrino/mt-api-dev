
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
#include <iomanip>
#include <string>

#include "common/helpers.h"
#include "mt-api.h"
#include "json.h"
#include "sql.h"

extern CMtApi*		g_mainInstance;
extern string		g_dataRoot;
extern bool		g_debugMode;
extern int		g_apiMode;
extern string		g_msgBoxText;
extern const char*	g_progNameShort;
extern const char*	g_progVersion;

CSql::CSql()
{
	Init();
}

void CSql::Init()
{
	mysqlCon = NULL;
	pwFile		= g_dataRoot + "/.passwd/sqlpasswd";
	usedDB		= "mediathek_1";
	const char* hostEnv = getenv("MT_API_DB_HOST");
	if (hostEnv && *hostEnv)
		mysqlHost = hostEnv;
	else
		mysqlHost = "127.0.0.1";
	tabChannelinfo	= "channelinfo";
	tabVersion	= "version";
	tabVideo	= "video";
	resultCount	= 0;
}

CSql::~CSql()
{
	if (mysqlCon != NULL)
		mysql_close(mysqlCon);
}

void CSql::show_error(const char* func, int line)
{
	std::ostringstream oss;
	oss << "<span style='color: OrangeRed'>[" << func << ':' << line
	    << "] Error(" << mysql_errno(mysqlCon) << ") ["
	    << mysql_sqlstate(mysqlCon) << "] \"" << mysql_error(mysqlCon)
	    << "\"\n<br /></span>";
	g_msgBoxText = oss.str();

	mysql_close(mysqlCon);
	mysqlCon = NULL;
}

bool CSql::connectMysql()
{
	string pw = readFile(pwFile);
	pw = trim(pw);
	vector<string> v = split(pw, ':');

	mysqlCon = mysql_init(NULL);
	unsigned long flags = 0;
//	flags |= CLIENT_MULTI_STATEMENTS;
//	flags |= CLIENT_COMPRESS;
	const char* host = mysqlHost.c_str();
	if (!mysql_real_connect(mysqlCon, host, v[0].c_str(), v[1].c_str(), usedDB.c_str(), 3306, NULL, flags)) {
		show_error(__func__, __LINE__);
		return false;
	}

	if (mysql_set_character_set(mysqlCon, "utf8") != 0) {
		show_error(__func__, __LINE__);
		return false;
	}

	return true;
}

string CSql::formatSql(string data, int id, string tagBefore, string tagAfter)
{
	string html = readFile(g_dataRoot + "/template/sql-format.html");
	data = base64encode(data);
	html = str_replace("@@@SQL_DATA@@@", data, html);
	html = str_replace("@@@ID@@@", to_string(id), html);

	return tagBefore + html + tagAfter;
}

double CSql::startTimer()
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	return (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
}

string CSql::getTimer(double startTime, string txt, int preci/*=3*/)
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	double workDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	std::ostringstream oss;
	oss.setf(std::ios::fixed);
	oss << txt << ' ' << std::setprecision(preci)
	    << ((workDTms - startTime) / 1000ULL) << " sec";
	return oss.str();
}

int CSql::row2int(MYSQL_ROW& row, uint64_t* lengths, int index)
{
	string tmp_s = row2string(row, lengths, index);
	return atoi(tmp_s.c_str());
}

bool CSql::row2bool(MYSQL_ROW& row, uint64_t* lengths, int index)
{
	string tmp_s = row2string(row, lengths, index);
	return (!strEqual(tmp_s.substr(0, 1), "0"));
}

string CSql::row2string(MYSQL_ROW& row, uint64_t* lengths, int index)
{
	string tmp_s = static_cast<string>(row[index]);
	return tmp_s.substr(0, lengths[index]);
}

int CSql::getResultCount(string where)
{
	if (mysqlCon == NULL)
		return 0;

	string sql = "";
        sql += "SELECT COUNT(id) AS anz FROM " + tabVideo + " " + where + ";";

//	double timer = startTimer();

	if (mysql_real_query(mysqlCon, sql.c_str(), sql.length()) != 0) {
		show_error(__func__, __LINE__);
		return false;
	}

	int ret = 0;
	MYSQL_RES* result = mysql_store_result(mysqlCon);
	if (result) {
		if (mysql_num_fields(result) > 0) {
			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			uint64_t* lengths = mysql_fetch_lengths(result);
			if ((row != NULL) && (row[0] != NULL)) {
				ret = row2int(row, lengths, 0);
			}
		}
		mysql_free_result(result);
	}

//	string timer_s = getTimer(timer, "Duration sql query 1: ");

	if (g_debugMode)
		g_mainInstance->htmlOut << formatSql(sql, 1, "", "") << endl;

	return ret;
}

bool CSql::sqlListVideo(cmdListVideo_t* clv, listVideoHead_t* lvh, vector<listVideo_t>& lv)
{
	if (mysqlCon == NULL)
		return false;

	time_t now      = (clv->refTime == 0) ? time(0) : clv->refTime;
	time_t fromTime = 0;
	time_t toTime   = 0;

	g_mainInstance->cjson->resetListVideoHeadStruct(lvh);
	lvh->refTime	= now;

	time_t epoch = clv->epoch;
	/* (clv->epoch < 0) => all data */
	if (epoch == 0)
		epoch = 1;

	if (epoch > 0) {
		epoch = epoch * 3600 * 24;
		fromTime = now;
		toTime   = now - epoch;
		if (clv->timeMode == timeMode_future)
			fromTime += epoch;
	}

	string where = "";
	where += " WHERE ( channel LIKE " + checkString(clv->channel, 128);
	where += " AND duration >= " + checkInt(clv->duration);
	if (epoch > 0) {
		where += " AND date_unix < " + checkInt(fromTime);
		where += " AND date_unix > " + checkInt(toTime);
	}
	else {
		if (clv->timeMode != timeMode_future)
			where += " AND date_unix < " + checkInt(now);
	}
	where += " )";

	resultCount = getResultCount(where);

//	double timer = startTimer();

	string sql0 = "";
	sql0 += "SELECT";
	sql0 += " channel, theme, title, description, website, subtitle, url, url_small, url_hd, url_rtmp,";
	sql0 += " url_rtmp_small, url_rtmp_hd, url_history, date_unix, duration, size_mb, geo, parse_m3u8";
	sql0 += " FROM " + tabVideo;
	sql0 += where;
	sql0 += " ORDER BY date_unix DESC";
	sql0 += " LIMIT " + checkInt(clv->limit);
	sql0 += " OFFSET " + checkInt(clv->start);

	string sql = "";
	sql += "SELECT * FROM ( ";
	sql += sql0;
	sql += " ) AS dingens";
	sql += " ORDER BY date_unix DESC, title ASC;";

	if (mysql_real_query(mysqlCon, sql.c_str(), sql.length()) != 0) {
		show_error(__func__, __LINE__);
		return false;
	}

	MYSQL_RES* result = mysql_store_result(mysqlCon);
	if (result) {
		if (mysql_num_fields(result) > 0) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result))) {
				listVideo_t lvv;
				g_mainInstance->cjson->resetListVideoStruct(&lvv);
				uint64_t* lengths = mysql_fetch_lengths(result);
				if ((row != NULL) && (row[0] != NULL)) {
					int index = 0;
					lvv.channel		= row2string(row, lengths, index++);
					lvv.theme		= row2string(row, lengths, index++);
					lvv.title		= row2string(row, lengths, index++);
					lvv.description		= row2string(row, lengths, index++);
					lvv.website		= row2string(row, lengths, index++);
					lvv.subtitle		= row2string(row, lengths, index++);
					lvv.url			= row2string(row, lengths, index++);
					lvv.url_small		= row2string(row, lengths, index++);
					lvv.url_hd		= row2string(row, lengths, index++);
					lvv.url_rtmp		= row2string(row, lengths, index++);
					lvv.url_rtmp_small	= row2string(row, lengths, index++);
					lvv.url_rtmp_hd		= row2string(row, lengths, index++);
					lvv.url_history		= row2string(row, lengths, index++);
					lvv.date_unix		= row2int(row, lengths, index++);
					lvv.duration		= row2int(row, lengths, index++);
					lvv.size_mb		= row2int(row, lengths, index++);
					lvv.geo			= row2string(row, lengths, index++);
					lvv.parse_m3u8		= row2int(row, lengths, index++);
				}
				lv.push_back(lvv);
			}
		}
		mysql_free_result(result);
	}

	int rowsCount = static_cast<int>(lv.size());
	lvh->start	= clv->start;
	lvh->end	= clv->start + rowsCount - 1;
	lvh->rows	= rowsCount;
	lvh->total	= resultCount;

//	string timer_s = getTimer(timer, "Duration sql query 2: ");

	if (g_debugMode)
		g_mainInstance->htmlOut << formatSql(sql, 2, "", "") << endl;

	return true;
}

bool CSql::sqlGetProgInfo(progInfo_t* pi)
{
	if (mysqlCon == NULL)
		return false;

	string sql = "";
	sql += "SELECT version, vdate, mvversion, mvdate, mventrys, progname, progversion";
	sql += " FROM " + tabVersion;
	sql += " LIMIT 1;";

	if (mysql_real_query(mysqlCon, sql.c_str(), sql.length()) != 0) {
		show_error(__func__, __LINE__);
		return false;
	}

	MYSQL_RES* result = mysql_store_result(mysqlCon);
	if (result) {
		if (mysql_num_fields(result) > 0) {
			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			uint64_t* lengths = mysql_fetch_lengths(result);
			if ((row != NULL) && (row[0] != NULL)) {
				int index = 0;
				pi->version	= row2string(row, lengths, index++);
				pi->vdate	= row2int(row, lengths, index++);
				pi->mvversion	= row2string(row, lengths, index++);
				pi->mvdate	= row2int(row, lengths, index++);
				pi->mventrys	= row2int(row, lengths, index++);
				pi->progname	= row2string(row, lengths, index++);
				pi->progversion	= row2string(row, lengths, index++);
			}
		}
		mysql_free_result(result);
	}

	if (g_debugMode)
		g_mainInstance->htmlOut << formatSql(sql, 1, "", "") << endl;

	pi->api		= static_cast<string>(g_progNameShort);
	pi->apiversion	= static_cast<string>(g_progVersion);
	return true;
}

bool CSql::sqlListLiveStreams(vector<livestreams_t>& ls)
{
	if (mysqlCon == NULL)
		return false;

	string sql = "";
	sql += "SELECT title, url, parse_m3u8";
	sql += " FROM " + tabVideo;
	sql += " WHERE (theme LIKE 'Livestream' AND title LIKE '%Livestream%')";
	sql += " ORDER BY channel, title ASC";
	sql += " LIMIT 50;";

	if (mysql_real_query(mysqlCon, sql.c_str(), sql.length()) != 0) {
		show_error(__func__, __LINE__);
		return false;
	}

	MYSQL_RES* result = mysql_store_result(mysqlCon);
	if (result) {
		if (mysql_num_fields(result) > 0) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result))) {
				livestreams_t lss;
				g_mainInstance->cjson->resetLiveStreamStruct(&lss);
				uint64_t* lengths = mysql_fetch_lengths(result);
				if ((row != NULL) && (row[0] != NULL)) {
					int index = 0;
					lss.title	= row2string(row, lengths, index++);
					lss.url		= row2string(row, lengths, index++);
					lss.parse_m3u8	= row2int(row, lengths, index++);
				}
				ls.push_back(lss);
			}
		}
		mysql_free_result(result);
	}

	if (g_debugMode)
		g_mainInstance->htmlOut << formatSql(sql, 1, "", "") << endl;

	return true;
}

bool CSql::sqlListChannels(vector<channels_t>& ch)
{
	if (mysqlCon == NULL)
		return false;

	string sql = "";
	sql += "SELECT channel, count, latest, oldest";
	sql += " FROM " + tabChannelinfo;
	sql += " ORDER BY channel ASC";
	sql += " LIMIT 50;";

	if (mysql_real_query(mysqlCon, sql.c_str(), sql.length()) != 0) {
		show_error(__func__, __LINE__);
		return false;
	}

	MYSQL_RES* result = mysql_store_result(mysqlCon);
	if (result) {
		if (mysql_num_fields(result) > 0) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result))) {
				channels_t chs;
				g_mainInstance->cjson->resetChannelStruct(&chs);
				uint64_t* lengths = mysql_fetch_lengths(result);
				if ((row != NULL) && (row[0] != NULL)) {
					int index = 0;
					chs.channel	= row2string(row, lengths, index++);
					chs.count	= row2int(row, lengths, index++);
					chs.latest	= row2int(row, lengths, index++);
					chs.oldest	= row2int(row, lengths, index++);
				}
				ch.push_back(chs);
			}
		}
		mysql_free_result(result);
	}

	if (g_debugMode)
		g_mainInstance->htmlOut << formatSql(sql, 1, "", "") << endl;

	return true;
}
