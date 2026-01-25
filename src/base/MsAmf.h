#ifndef MS_AMF_H
#define MS_AMF_H
#include <any>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

#define AMF0_STRING_MARKER 0x02
#define AMF0_NUMBER_MARKER 0x00
#define AMF0_OBJECT_MARKER 0x03
#define AMF0_BOOLEAN_MARKER 0x01
#define AMF0_NULL_MARKER 0x05
#define AMF0_UNDEFINED_MARKER 0x06
#define AMF0_ECMA_ARRAY_MARKER 0x08

struct AmfItem {
	uint8_t type;
	std::any value;
};

struct AmfObject {
	int decode(const uint8_t *data, size_t dataLen, size_t &offset);
	void encode(std::vector<uint8_t> &outBuf);

	bool is_null() { return isNull; }
	void set_null() { isNull = true; }
	bool is_undefined() { return isUndefined; }
	void set_undefined() { isUndefined = true; }

	string get_key_string(const string &key);
	double get_key_number(const string &key);
	bool get_key_boolean(const string &key);
	void set_key_string(const string &key, const string &value);
	void set_key_number(const string &key, double value);
	void set_key_boolean(const string &key, bool value);

	std::map<string, AmfItem> properties;
	bool isNull = false;
	bool isUndefined = false;
};

struct AmfScriptData {
	int decode(const uint8_t *data, size_t dataLen, size_t &offset);
	std::vector<AmfItem> properties;
};

int amf_decode_utf8(const uint8_t *data, size_t dataLen, size_t &offset, string &outStr);

int amf_decode_string(const uint8_t *data, size_t dataLen, size_t &offset, string &outStr);

int amf_decode_number(const uint8_t *data, size_t dataLen, size_t &offset, double &outNum);

int amf_decode_boolean(const uint8_t *data, size_t dataLen, size_t &offset, bool &outBool);

int amf_decode_object(const uint8_t *data, size_t dataLen, size_t &offset,
                      std::map<string, AmfItem> &outObj);

void amf_encode_utf8(std::vector<uint8_t> &outBuf, const string &str);
void amf_encode_string(std::vector<uint8_t> &outBuf, const string &str);
void amf_encode_number(std::vector<uint8_t> &outBuf, double val);
void amf_encode_boolean(std::vector<uint8_t> &outBuf, bool val);
void amf_encode_object(std::vector<uint8_t> &outBuf, std::map<string, AmfItem> &obj);

#endif // MS_AMF_H