
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include "SuperStream.h"

using namespace android;

int main(int argc, char** argv) {
    //hardware::ProcessState::self()->setThreadPoolConfiguration(1, true /*callerJoinsPool*/);
    // start the thread pool
    sp<ProcessState> ps(ProcessState::self());
    ps->setThreadPoolMaxThreadCount(1);
    ps->startThreadPool();

    // instantiate surfaceflinger
    sp<SuperStream> stream = new SuperStream();

    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SuperStream::getServiceName()), stream, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL);

    IPCThreadState::self()->joinThreadPool();
    //hardware::IPCThreadState::self()->joinThreadPool(true);

    return 0;
}
