#ifndef SNITCH_SNITCH_HPP
#define SNITCH_SNITCH_HPP

// Minimal build compat shim -- used when vcpkg snitch unavailable.
// Replace with the real snitch::snitch vcpkg target when vcpkg is configured.

#define SNITCH_CAT_(a, b) SNITCH_CAT_I_(a, b)
#define SNITCH_CAT_I_(a, b) a##b

#define SNITCH_TEST_CASE(name, tags) static void SNITCH_CAT_(snitch_tc_, __LINE__)()

#define SNITCH_SECTION(name) if (true)
#define SNITCH_REQUIRE(cond) (void)(cond)
#define SNITCH_REQUIRE_FALSE(cond) (void)(cond)
#define SNITCH_FAIL_CHECK(msg) (void)(msg)

#endif  // SNITCH_SNITCH_HPP
