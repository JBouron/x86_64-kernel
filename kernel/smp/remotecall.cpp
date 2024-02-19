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
        Ptr<CallDesc> const desc(data.remoteCallQueue[0]);
        data.remoteCallQueue.erase(0);

        data.remoteCallQueueLock.unlock();

        desc->invoke();

        data.remoteCallQueueLock.lock();
    }
    data.remoteCallQueueLock.unlock();

    data.isProcessingRemoteCallQueue = false;
}

// Has Init() been called already? Used to assert that cpus are not trying to
// use the namespace before its initialization.
static bool IsInitialized = false;

// Initialize the RemoteCall subsystem.
void Init() {
    Interrupts::registerHandler(Interrupts::VectorMap::RemoteCallVector,
                                handleRemoteCallInterrupt);
    IsInitialized = true;
}

// Check if the Smp::RemoteCall namespace has been initialized. This is only
// meant to be used by internal functions.
bool _isInitialized() {
    return IsInitialized;
}

}
