#ifndef MEMORY_ARENA_HANDLER_H
#define MEMORY_ARENA_HANDLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	// Opaque handle to the C++ ArenaHandler class
	typedef struct CArenaHandler CArenaHandler;

	// Mirror of the C++ ErrorCode enum
	typedef enum
	{
		ARENA_SUCCESS = 0,
		ARENA_OUT_OF_MEMORY = 1,
		ARENA_INSUFFICIENT_RESOURCE = 2
	} ArenaErrorCode;

	/**
	 * @brief Creates a new ArenaHandler instance.
	 * @return Pointer to the handler, or NULL on allocation failure.
	 */
	CArenaHandler* arena_create(void);

	/**
	 * @brief Destroys the ArenaHandler and frees all managed memory.
	 */
	void arena_destroy(CArenaHandler* handler);

	/**
	 * @brief Requests memory from the arena.
	 * * @param handler The arena handler instance.
	 * @param size Number of bytes to allocate.
	 * @param alignment Alignment in bytes.
	 * @param use_default_allocation If true, allocates larger blocks by default.
	 * @return Pointer to the allocated memory, or NULL on failure.
	 */
	void* arena_request_memory(CArenaHandler* handler, size_t size, uint8_t alignment,
		bool use_default_allocation);

	/**
	 * @brief Frees memory back to the arena (specifically to the free list).
	 * * @param handler The arena handler instance.
	 * @param ptr Pointer to the memory to free.
	 * @param size Size of the memory block being freed.
	 * @return ArenaErrorCode indicating success or failure.
	 */
	ArenaErrorCode arena_free(CArenaHandler* handler, void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_ARENA_HANDLER_H
