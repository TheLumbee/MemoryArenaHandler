add_library(memory_arena_handler
	"memory_arena_handler.cpp"
)

enable_testing()
include(GoogleTest)

add_executable(memory_arena_handler_test
	"test/memory_arena_handler_test.cpp"
)

target_link_libraries(memory_arena_handler_test
	memory_arena_handler
	GTest::gtest_main
)

gtest_discover_tests(memory_arena_handler_test)
