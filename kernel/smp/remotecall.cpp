#include <smp/remotecall.hpp>
#include <interrupts/interrupts.hpp>
#include <logging/log.hpp>

namespace Smp::RemoteCall {

// Interrupt handler for a RemoteCall interrupt. Processes all CallDesc
// currently enqueued in the remoteCallQueue of this cpu.
static void handleRemoteCallInterrupt(
    __attribute__((unused)) Interrupts::Vector const vec,
    __attribute__((unused)) Interrupts::Frame const& frame) {
    Smp::PerCpu::Data& data(Smp::PerCpu::data());

    // In order to guarantee that remote functions are executed in the same
    // order they are enqueued, we need to avoid nested processing of the queue.
    if (data.isProcessingRemoteCallQueue) {
        return;
    }
    data.isProcessingRemoteCallQueue = true;

    data.remoteCallQueueLock.lock();
    while (data.remoteCallQueue.size()) {
        CallDesc* const desc(data.remoteCallQueue[0]);
        data.remoteCallQueue.erase(0);

        data.remoteCallQueueLock.unlock();

        desc->invoke();
        delete desc;

        data.remoteCallQueueLock.lock();
    }
    data.remoteCallQueueLock.unlock();

    data.isProcessingRemoteCallQueue = false;
}

// Initialize the RemoteCall subsystem.
void Init() {
    static bool remoteCallInitialized = false;
    if (!remoteCallInitialized) {
        Interrupts::registerHandler(Interrupts::VectorMap::RemoteCallVector,
                                    handleRemoteCallInterrupt);
    }
}

}
