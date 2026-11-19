#include "release_notice.hpp"
#include "anaiero_release_notice.hpp"
namespace anaiero::deployment {
// The adapter itself is hashed in the state contract. Only its restricted,
// generated display literal is outside that contract and inside provenance.
std::string_view release_notice() noexcept{return compiled::release_notice;}
}
