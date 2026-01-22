#ifndef MS_MSG_H
#define MS_MSG_H

#include <any>
#include <string>

using namespace std;

enum MS_SYS_MSG_ID {
	MS_EXIT = 1,

};

class MsMsg {
public:
	int m_sessionID = 0;
	int m_msgID = 0;
	int m_srcType = 0;
	int m_srcID = 0;
	int m_dstType = 0;
	int m_dstID = 0;

	int m_intVal = 0;
	string m_strVal;
	std::any m_any;
};

#endif // MS_MSG_H
