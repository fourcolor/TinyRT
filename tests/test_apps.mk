# Regression apps that are expected to build with the current public API.
#
# Older app tests that still use pre-handle object pointers stay outside this
# list until they are ported.
APP_TESTS ?= destroy_test handle_object_test heap_self_test preempt_rr_test
