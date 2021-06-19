#include <fstream>
#include "imp/rapidjson/document.h"
#include "imp/rapidjson/stringbuffer.h"
#include "imp/rapidjson/writer.h"

#define RAPIDJSON_HAS_STDSTRING 1

using namespace rapidjson;

Document document;


void cfgdump() {
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);

	document.Accept(writer);
}

Document init_cfg() {
	Document document;
	document.SetObject();
	Document::AllocatorType& allocator = document.GetAllocator();

	return document;
}

Document create_bb_json() {
	Document bb_info;
	bb_info.SetObject();
	return bb_info;
}