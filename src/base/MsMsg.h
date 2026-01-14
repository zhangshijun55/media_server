#ifndef MS_MSG_H
#define MS_MSG_H

#include "MsOsConfig.h"
#include <any>
#include <string>

using namespace std;

enum MS_SYS_MSG_ID {
	MS_EXIT = 1,

};

class MsMsg {
public:
	MsMsg();

	int m_sessinID;
	int m_msgID;
	int m_srcType;
	int m_srcID;
	int m_dstType;
	int m_dstID;

	int m_intVal;
	string m_strVal;
	std::any m_any;
};

#endif // MS_MSG_H
