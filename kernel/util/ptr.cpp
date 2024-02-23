#include <util/ptr.hpp>

// See comment in Ptr<T>::Ptr<T>(T*) for why this is needed.
Atomic<u64> _nullPtrRefCnt;
