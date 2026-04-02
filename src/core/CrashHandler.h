#pragma once

namespace vega {

/// Install a Windows Structured Exception Handler that writes a minidump
/// on unhandled exceptions, then shows a MessageBox with the dump path.
void installCrashHandler();

} // namespace vega
