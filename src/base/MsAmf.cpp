#include "MsAmf.h"
#include "MsLog.h"

int AmfObject::decode(const uint8_t *data, size_t dataLen, size_t &offset) {
	if (dataLen <= offset) {
		return -1;
	}
	if (data[offset] == AMF0_NULL_MARKER) {
		isNull = true;
		offset += 1;
		return 0;
	}
	if (data[offset] == AMF0_UNDEFINED_MARKER) {
		isUndefined = true;
		offset += 1;
		return 0;
	}

	return amf_decode_object(data, dataLen, offset, properties);
}

void AmfObject::encode(std::vector<uint8_t> &outBuf) {
	if (isNull) {
		outBuf.push_back(AMF0_NULL_MARKER);
		return;
	}
	if (isUndefined) {
		outBuf.push_back(AMF0_UNDEFINED_MARKER);
		return;
	}
	amf_encode_object(outBuf, properties);
}

string AmfObject::get_key_string(const string &key) {
	auto it = properties.find(key);
	if (it != properties.end() && it->second.type == AMF0_STRING_MARKER) {
		return std::any_cast<string>(it->second.value);
	}
	return "";
}

double AmfObject::get_key_number(const string &key) {
	auto it = properties.find(key);
	if (it != properties.end() && it->second.type == AMF0_NUMBER_MARKER) {
		return std::any_cast<double>(it->second.value);
	}
	return 0.0;
}

bool AmfObject::get_key_boolean(const string &key) {
	auto it = properties.find(key);
	if (it != properties.end() && it->second.type == AMF0_BOOLEAN_MARKER) {
		return std::any_cast<bool>(it->second.value);
	}
	return false;
}

void AmfObject::set_key_string(const string &key, const string &value) {
	properties[key] = {AMF0_STRING_MARKER, value};
}

void AmfObject::set_key_number(const string &key, double value) {
	properties[key] = {AMF0_NUMBER_MARKER, value};
}

void AmfObject::set_key_boolean(const string &key, bool value) {
	properties[key] = {AMF0_BOOLEAN_MARKER, value};
}

int amf_decode_utf8(const uint8_t *data, size_t dataLen, size_t &offset, string &outStr) {
	if (offset + 2 > dataLen)
		return -1;

	uint16_t strLen = (data[offset] << 8) | (data[offset + 1]);
	offset += 2;

	if (offset + strLen > dataLen)
		return -1;

	outStr.assign((const char *)data + offset, strLen);
	offset += strLen;

	return 0;
}

int amf_decode_string(const uint8_t *data, size_t dataLen, size_t &offset, string &outStr) {

	if (offset + 3 > dataLen)
		return -1;

	if (data[offset] != AMF0_STRING_MARKER) // AMF0 String marker
		return -1;

	uint16_t strLen = (data[offset + 1] << 8) | (data[offset + 2]);
	offset += 3;

	if (offset + strLen > dataLen)
		return -1;

	outStr.assign((const char *)data + offset, strLen);
	offset += strLen;

	return 0;
}

int amf_decode_number(const uint8_t *data, size_t dataLen, size_t &offset, double &outNum) {
	if (offset + 9 > dataLen)
		return -1;

	if (data[offset] != AMF0_NUMBER_MARKER) // AMF0 Number marker
		return -1;

	uint64_t numBits = 0;
	for (int i = 0; i < 8; i++) {
		numBits = (numBits << 8) | data[offset + 1 + i];
	}
	offset += 9;

	double *numPtr = reinterpret_cast<double *>(&numBits);
	outNum = *numPtr;

	return 0;
}

int amf_decode_boolean(const uint8_t *data, size_t dataLen, size_t &offset, bool &outBool) {
	if (offset + 2 > dataLen)
		return -1;

	if (data[offset] != AMF0_BOOLEAN_MARKER) // AMF0 Boolean marker
		return -1;

	outBool = (data[offset + 1] != 0);
	offset += 2;

	return 0;
}

int amf_decode_object_internal(const uint8_t *data, size_t dataLen, size_t &offset,
                               std::map<string, AmfItem> &outObj) {
	// Further object decoding would go here
	while (offset < dataLen) {
		// Check for object end marker
		if (offset + 3 <= dataLen && data[offset] == 0x00 && data[offset + 1] == 0x00 &&
		    data[offset + 2] == 0x09) {
			offset += 3;
			break;
		}
		// Decode key (string)
		string key;
		std::any value;

		if (amf_decode_utf8(data, dataLen, offset, key) < 0) {
			return -1;
		}

		// Decode value (could be various types, here we just skip for simplicity)
		if (offset >= dataLen) {
			return -1;
		}
		uint8_t valueType = data[offset];
		// Skip value based on type (not fully implemented)
		switch (valueType) {
		case AMF0_NUMBER_MARKER: // Number
		{
			double numVal;
			if (amf_decode_number(data, dataLen, offset, numVal) < 0) {
				return -1;
			}
			value = numVal;
			outObj[key] = {valueType, value};
			// MS_LOG_DEBUG("AMF0 Number: key=%s value=%.2f", key.c_str(), numVal);
		} break;
		case AMF0_BOOLEAN_MARKER: // Boolean
		{
			bool boolVal;
			if (amf_decode_boolean(data, dataLen, offset, boolVal) < 0) {
				return -1;
			}
			value = boolVal;
			outObj[key] = {valueType, value};
			// MS_LOG_DEBUG("AMF0 Boolean: key=%s value=%d", key.c_str(), boolVal);
		} break;
		case AMF0_STRING_MARKER: // String
		{
			string strVal;
			if (amf_decode_string(data, dataLen, offset, strVal) < 0) {
				return -1;
			}
			value = strVal;
			outObj[key] = {valueType, value};
			// MS_LOG_DEBUG("AMF0 String: key=%s value=%s", key.c_str(), strVal.c_str());
		} break;

		// Add more cases as needed
		default:
			MS_LOG_DEBUG("Unsupported AMF0 type: %d", valueType);
			break; // Unsupported type for now
		}
	}

	return 0;
}

int amf_decode_object(const uint8_t *data, size_t dataLen, size_t &offset,
                      std::map<string, AmfItem> &outObj) {
	if (offset + 1 > dataLen) {
		return -1;
	}

	if (data[offset] != AMF0_OBJECT_MARKER) // AMF0 Object marker
		return -1;

	offset += 1;

	return amf_decode_object_internal(data, dataLen, offset, outObj);
}

void amf_encode_utf8(std::vector<uint8_t> &outBuf, const string &str) {
	outBuf.push_back((str.size() >> 8) & 0xFF);
	outBuf.push_back(str.size() & 0xFF);
	outBuf.insert(outBuf.end(), str.begin(), str.end());
}

void amf_encode_string(std::vector<uint8_t> &outBuf, const string &str) {
	outBuf.push_back(AMF0_STRING_MARKER);
	outBuf.push_back((str.size() >> 8) & 0xFF);
	outBuf.push_back(str.size() & 0xFF);
	outBuf.insert(outBuf.end(), str.begin(), str.end());
}

void amf_encode_number(std::vector<uint8_t> &outBuf, double val) {
	outBuf.push_back(AMF0_NUMBER_MARKER);
	uint64_t *ptr = reinterpret_cast<uint64_t *>(&val);
	uint64_t uval = *ptr;
	for (int i = 7; i >= 0; --i) {
		outBuf.push_back((uval >> (i * 8)) & 0xFF);
	}
}

void amf_encode_boolean(std::vector<uint8_t> &outBuf, bool val) {
	outBuf.push_back(AMF0_BOOLEAN_MARKER);
	outBuf.push_back(val ? 0x01 : 0x00);
}

void amf_encode_object(std::vector<uint8_t> &outBuf, std::map<string, AmfItem> &obj) {
	outBuf.push_back(AMF0_OBJECT_MARKER);
	for (const auto &pair : obj) {
		const string &key = pair.first;
		const AmfItem &item = pair.second;

		// Encode key
		amf_encode_utf8(outBuf, key);

		// Encode value based on type
		switch (item.type) {
		case AMF0_STRING_MARKER:
			amf_encode_string(outBuf, std::any_cast<string>(item.value));
			break;
		case AMF0_NUMBER_MARKER:
			amf_encode_number(outBuf, std::any_cast<double>(item.value));
			break;
		case AMF0_BOOLEAN_MARKER:
			amf_encode_boolean(outBuf, std::any_cast<bool>(item.value));
			break;
		// Add more cases as needed
		default:
			MS_LOG_DEBUG("Unsupported AMF0 type for encoding: %d", item.type);
			break; // Unsupported type for now
		}
	}
	// Object end marker
	outBuf.push_back(0x00);
	outBuf.push_back(0x00);
	outBuf.push_back(0x09);
}

int AmfScriptData::decode(const uint8_t *data, size_t dataLen, size_t &offset) {
	if (offset + 1 > dataLen) {
		return -1;
	}

	while (offset < dataLen) {

		// Decode value (could be various types, here we just skip for simplicity)
		if (offset >= dataLen) {
			return -1;
		}
		uint8_t valueType = data[offset];
		std::any value;
		// Skip value based on type (not fully implemented)
		switch (valueType) {
		case AMF0_NUMBER_MARKER: // Number
		{
			double numVal;
			if (amf_decode_number(data, dataLen, offset, numVal) < 0) {
				return -1;
			}
			value = numVal;
			properties.push_back({valueType, value});
			// MS_LOG_DEBUG("AMF0 ScriptData Number: key=%d value=%.2f", properties.size() - 1,
			//              numVal);
		} break;

		case AMF0_BOOLEAN_MARKER: // Boolean
		{
			bool boolVal;
			if (amf_decode_boolean(data, dataLen, offset, boolVal) < 0) {
				return -1;
			}
			value = boolVal;
			properties.push_back({valueType, value});
			// MS_LOG_DEBUG("AMF0 ScriptData Boolean: key=%d value=%d", properties.size() - 1,
			//              boolVal);
		} break;

		case AMF0_STRING_MARKER: // String
		{
			string strVal;
			if (amf_decode_string(data, dataLen, offset, strVal) < 0) {
				return -1;
			}
			value = strVal;
			properties.push_back({valueType, value});
			// MS_LOG_DEBUG("AMF0 ScriptData String: key=%d value=%s", properties.size() - 1,
			//              strVal.c_str());
		} break;

		case AMF0_OBJECT_MARKER: // Object
		{
			std::map<string, AmfItem> objVal;
			if (amf_decode_object(data, dataLen, offset, objVal) < 0) {
				return -1;
			}
			value = objVal;
			properties.push_back({valueType, value});
			// MS_LOG_DEBUG("AMF0 ScriptData Object: key=%d decoded", properties.size() - 1);
		} break;

		case AMF0_ECMA_ARRAY_MARKER: // ECMA Array
		{
			offset += 5; // Skip
			std::map<string, AmfItem> objVal;
			if (amf_decode_object_internal(data, dataLen, offset, objVal) < 0) {
				return -1;
			}
			value = objVal;
			properties.push_back({valueType, value});
			// MS_LOG_DEBUG("AMF0 ScriptData ECMA Array: key=%d decoded", properties.size() - 1);
		} break;
		// Add more cases as needed
		default:
			MS_LOG_DEBUG("Unsupported AMF0 ScriptData type: %d", valueType);
			break; // Unsupported type for now
		}
	}

	return 0;
}