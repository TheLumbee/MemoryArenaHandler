#ifndef MEMORY_ARENA_HANDLER_H
#define MEMORY_ARENA_HANDLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef ARENA_BUILD_LIB
#define ARENA_API __declspec(dllexport)
#else
#define ARENA_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
// GCC/Clang (Linux/Mac/BSD)
#define ARENA_API __attribute__((visibility("default")))
#else
// Fallback for unknown compilers
#define ARENA_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	// Opaque handle
	typedef struct CArenaHandler CArenaHandler;

	typedef enum
	{
		ARENA_SUCCESS = 0,
		ARENA_OUT_OF_MEMORY = 1,
		ARENA_INSUFFICIENT_RESOURCE = 2
	} ArenaErrorCode;

	// Apply the macro to every function declaration

	ARENA_API CArenaHandler* arena_create(void);

	ARENA_API void arena_destroy(CArenaHandler* handler);

	ARENA_API void* arena_alloc(CArenaHandler* handler, size_t size,
		uint8_t alignment, bool use_default_allocation);

	ARENA_API ArenaErrorCode arena_free(
		CArenaHandler* handler, void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_ARENA_HANDLER_H
