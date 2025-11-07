
#ifndef __JSON_H__
#define __JSON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <jsoncpp/json/json.h>

#include <string>

#include "types.h"

using namespace std;

class CJson
{
	private:

		string cooliSig1;
		string cooliSig2;
		string cooliSig3;

		void Init();
		void errorMsg(const char* func, int line, string msg="");
		void parseError(const char* func, int line, string msg="");
		void resetQueryHeaderStruct(query_header_t* qh);
		void resetCmdListVideoStruct(cmdListVideo_t* lv);
		bool parseListVideo(Json::Value root);
		bool asBool(Json::Value::iterator it);

	public:

		CJson();
		~CJson();

		listVideoHead_t listVideoHead;
		vector<listVideo_t> listVideo_v;

		void resetProgInfoStruct(progInfo_t* pi);
		void resetLiveStreamStruct(livestreams_t* ls);
		void resetChannelStruct(channels_t* ch);
		void resetListVideoStruct(listVideo_t* lv);
		void resetListVideoHeadStruct(listVideoHead_t* lvh);
		bool parsePostData(string jData);
		string styledJson(string json);
		string styledJson(Json::Value json);
		string progInfo2Json(progInfo_t* pi, string indent="");
		string liveStreamList2Json(vector<livestreams_t>& ls, string indent="");
		string channelList2Json(vector<channels_t>& ch, string indent="");
		string videoList2Json(string indent="");
		string jsonErrMsg(string msg, int err=1);
		string json2String(Json::Value json, string indent="");
		string formatJson(string data, string tagBefore="", string tagAfter="");
};



#endif // __JSON_H__
