#include "MsApp.h"
#include "MsConfig.h"
#include "MsDbMgr.h"
#include "MsDevMgr.h"
#include "MsGbServer.h"
#include "MsHttpServer.h"
#include "MsHttpStream.h"
#include "MsJtServer.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include "MsRtmpServer.h"
#include "MsRtspSink.h"
#include "MsThreadPool.h"
#include "MsTimer.h"
#include <signal.h>

#if ENABLE_RTC
#include "MsRtcServer.h"
#endif

int main(int argc, char *argv[]) {
	signal(SIGPIPE, SIG_IGN);

	MsThreadPool::Instance().spawn(4);
	MsConfig *config = MsConfig::Instance();
	config->LoadConfig();

	for (int i = 1; i < argc; i++) {
		if (string(argv[i]) == "-bind_ip" && i + 1 < argc) {
			config->SetConfigStr("localBindIP", argv[i + 1]);
			i++; // Skip the argument value
		}
	}

	MsLog::Instance()->SetLevel(config->GetConfigInt("logLevel"));
	MsLog::Instance()->Run();

	MsTimer::Instance()->Run();

	if (MsDbMgr::Instance()->Init()) {
		printf("init db mgr failed\n");
		return -1;
	}

	MsDevMgr::Instance()->LoadDevice();

	shared_ptr<MsReactor> commReactor = make_shared<MsReactor>(MS_COMMON_REACTOR, 1);
	commReactor->Run();

	shared_ptr<MsHttpServer> httpServer = make_shared<MsHttpServer>(MS_HTTP_SERVER, 1);
	httpServer->Run();

	shared_ptr<MsGbServer> gbServer = make_shared<MsGbServer>(MS_GB_SERVER, 1);
	gbServer->Run();

	shared_ptr<MsRtspServer> rtsp = make_shared<MsRtspServer>(MS_RTSP_SERVER, 1);
	rtsp->Run();

	shared_ptr<MsHttpStream> hs = make_shared<MsHttpStream>(MS_HTTP_STREAM, 1);
	hs->Run();

	// shared_ptr<MsJtServer> jt = make_shared<MsJtServer>(MS_JT_SERVER, 1);
	// jt->Run();

	shared_ptr<MsRtmpServer> rtmpServer = make_shared<MsRtmpServer>(MS_RTMP_SERVER, 1);
	rtmpServer->Run();

#if ENABLE_RTC
	shared_ptr<MsRtcServer> rtcServer = make_shared<MsRtcServer>(MS_RTC_SERVER, 1);
	rtcServer->Run();
#endif

	printf("media server v1.0.0 running\n");

	MsApp::Instance()->Run();

	return 0;
}
