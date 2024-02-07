// Definition of the Err type.
#pragma once

#include <util/assert.hpp>
#include <util/error.hpp>
#include <selftests/selftests.hpp>

struct Ok_t {};

// Value/tag to construct an Err that does not contain an error.
inline Ok_t Ok;

// Type representing an error or the lack thereof.
// An Err optionally contains an error. It is used for functions that typically
// return void, but still need to communicate whether or not an error occured.
// For functions that need to both return a non-void value _and_ communicate
// errors use Res<T> instead.
// In case of an error an Err contains an Error value, otherwise it contains no
// value.
class Err {
public:
    // A default Err does not contain an error.
    Err() : m_isError(false) {}

    // Construct an Err that does not contain an Error.
    Err(Ok_t const) : m_isError(false) {}
    
    // Construct an Err that contains an Error.
    // @param error: The error to be contained in the resulting Err.
    Err(Error const error) : m_isError(true), m_error(error) {}

    // Check if this Err contains an error.
    // @return: true if it contains an error, false otherwise.
    explicit operator bool() const {
        return m_isError;
    }

    // Get the error contained in this Err. If this Err does not contain an
    // error then this function PANICs.
    // @return: The contained Error.
    Error error() const {
        if (!m_isError) {
            ASSERT(!"Attempt to call error() on an Err with m_isError==false");
        }
        return m_error;
    }
private:
    // true if this Err contains an error, false otherwise.
    bool m_isError;
    // The error contained. Undefined if m_isError == false.
    Error m_error;
};

namespace ErrType {
// Run the tests for the Err type.
void Test(SelfTests::TestRunner& runner);
}
