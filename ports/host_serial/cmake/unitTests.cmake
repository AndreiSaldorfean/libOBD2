# host_serial drives a real K-line adapter, so it has no unit-test build.
# The library is unit-tested on the virtual line: set "PLATFORM": "host_virtual" in build_config.json.
message(FATAL_ERROR "PLATFORM=host_serial has no unit tests; use PLATFORM=host_virtual for the test build")
