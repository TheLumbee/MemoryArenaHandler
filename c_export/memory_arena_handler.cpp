#include "memory_arena_handler.h"
#include "memory_arena_handler.hpp"

#include <cstring>

static inline mem_arena_handler::ArenaHandler* to_cpp(CArenaHandler* handler)
{
	return (mem_arena_handler::ArenaHandler*)handler;
}

extern "C"
{
	CArenaHandler* arena_create()
	{
		mem_arena_handler::ArenaHandler* handler =
			(mem_arena_handler::ArenaHandler*)malloc(
				sizeof(mem_arena_handler::ArenaHandler));
		if (handler == nullptr)
		{
			return nullptr;
		}

		memset((void*)handler, 0, sizeof(mem_arena_handler::ArenaHandler));
		return (CArenaHandler*)handler;
	}

	void arena_destroy(CArenaHandler* handler)
	{
		mem_arena_handler::ArenaHandler* arena_handler = to_cpp(handler);
		arena_handler->mem_arena_handler::ArenaHandler::~ArenaHandler();
		free(handler);
	}

	void* arena_request_memory(CArenaHandler* handler, size_t size,
		uint8_t alignment, bool use_default_allocation)
	{
		mem_arena_handler::ArenaHandler* arena_handler = to_cpp(handler);
		return arena_handler->request_memory(
			size, alignment, use_default_allocation);
	}

	ArenaErrorCode arena_free(CArenaHandler* handler, void* ptr, size_t size)
	{
		mem_arena_handler::ArenaHandler* arena_handler = to_cpp(handler);
		switch (arena_handler->free_memory(ptr, size))
		{
			case (mem_arena_handler::ErrorCode::Success):
			{
				return ARENA_SUCCESS;
			}

			case (mem_arena_handler::ErrorCode::InsufficientResource):
			{
				return ARENA_INSUFFICIENT_RESOURCE;
			}

			case (mem_arena_handler::ErrorCode::OutOfMemory):
			{
				return ARENA_OUT_OF_MEMORY;
			}
		}
	}
}
