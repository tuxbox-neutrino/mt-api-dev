
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <tidy.h>
#include <tidybuffio.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <climits>
#include <string>

#include "common/helpers.h"
#include "html.h"

extern const char*	g_progName;
extern const char*	g_progVersion;
extern const char*	g_progCopyright;
extern string		g_dataRoot;
extern string		g_msgBoxText;

CHtml::CHtml()
{
	Init();
}

void CHtml::Init()
{
}

CHtml::~CHtml()
{
}

string CHtml::tidyRepair(string html, int displayError/*=0*/)
{
	(void)displayError;
	return html;
}

string CHtml::getHtmlHeader(string title, int flags, string extraHeader/*=""*/)
{
	string ret = " \
	<!DOCTYPE html>\n \
	<html><head>\n \
	<meta charset=\"utf-8\">\n";
	if ((flags & includeGenerator) == includeGenerator) {
#ifdef USE_CLANG
	string gcc_vers = "CLANG " + std::to_string(__clang_major__) + "." +
			  std::to_string(__clang_minor__) + "." + std::to_string(__clang_patchlevel__);
#else
	string gcc_vers = static_cast<string>(__VERSION__);
	size_t pos = gcc_vers.find_first_of(" ");
	if (pos != string::npos)
		gcc_vers = gcc_vers.substr(0, pos);
	gcc_vers = "GCC " + gcc_vers;
#endif
	string sass_vers;
#ifdef SASS_VERSION
	sass_vers = ", SASS " + string(SASS_VERSION);
#endif
	string tidy_vers = " and HTML Tidy for Linux v" + string(tidyLibraryVersion());
		ret += " \
		<meta name=\"generator\" content=\"" + gcc_vers + sass_vers + tidy_vers + "\" />\n";
	}
	if ((flags & includeCopyR) == includeCopyR) {
		ret += " \
		<meta name=\"copyright\" content=\"" + static_cast<string>(g_progCopyright) + "\" />\n";
	}
	if ((flags & includeApplication) == includeApplication) {
		ret += " \
		<meta name=\"application\" content=\"" + static_cast<string>(g_progName) + " " + g_progVersion + "\" />\n";
	}
	if ((flags & includeNoCache) == includeNoCache) {
		ret += " \
		<meta http-equiv=\"cache-control\" content=\"no-cache\" />\n \
		<meta http-equiv=\"Pragma\" content=\"no-cache\" />\n \
		<meta http-equiv=\"Expires\" content=\"0\" />\n";
	}
	if (!extraHeader.empty()) {
		ret += extraHeader;
	}
	ret += " \
	<link href=\"/images/favicon02.ico\" rel=\"icon\" type=\"image/x-icon\" />\n \
	<title>" + title + "</title>\n";
	return ret;
}

string CHtml::getHtmlFooter(string templ, string tagBefore)
{
	return tagBefore + readFile(templ);
}

string CHtml::getIndexSite()
{
	stringstream ret;
	int headerFlags = 0;
	headerFlags |= CHtml::includeCopyR;
	headerFlags |= CHtml::includeGenerator;
	headerFlags |= CHtml::includeApplication;
	string extraHeader = " \
		<link rel=\"stylesheet\" type=\"text/css\" href=\"/css/index.css\" />\n \
		";
	ret << getHtmlHeader("Coolithek", headerFlags, extraHeader);
	string html = readFile(g_dataRoot + "/template/index.html");
	ret << html << endl;
	ret << "</body></html>" << endl;
	return ret.str();
}

string CHtml::getErrorSite(int errNum, string site)
{
	stringstream ret;
	int headerFlags = 0;
	headerFlags |= CHtml::includeCopyR;
	headerFlags |= CHtml::includeGenerator;
	headerFlags |= CHtml::includeApplication;
	string extraHeader = " \
		<link rel=\"stylesheet\" type=\"text/css\" href=\"/css/error.css\" />\n \
		";
	ret << getHtmlHeader("Coolithek - Error " + std::to_string(errNum), headerFlags, extraHeader);
	string html = readFile(g_dataRoot + "/template/error.html");

	string errText;
	if (errNum == 403) {
		errText = "Verwehrt der Zugang dir ist.";
	}
	else if (errNum == 404) {
		string site_ = (site.empty()) ? "eine Seite" : "die <span class='errorText3'>" + site + ".html</span>";
		errText = "Verloren " + site_ + " du hast.<br />Wie peinlich. Wie peinlich!";
	}
	else {
		errText = "Unbekannt der Fehler mir ist.";
	}

	html = str_replace("@@@ERR_NUM@@@", to_string(errNum), html);
	ret << str_replace("@@@ERR_TXT@@@", errText, html) << endl;
	ret << "</body></html>" << endl;
	return ret.str();
}
