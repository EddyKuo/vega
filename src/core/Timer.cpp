#include "core/Timer.h"
#include "core/Logger.h"

namespace vega
{

ScopeTimer::~ScopeTimer()
{
    VEGA_LOG_DEBUG("[Timer] {} : {:.3f} ms", name_, timer_.elapsed_ms());
}

} // namespace vega
