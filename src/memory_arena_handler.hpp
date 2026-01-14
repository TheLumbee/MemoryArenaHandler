#ifndef MEMORY_ARENA_HANDLER_HPP
#define MEMORY_ARENA_HANDLER_HPP

#include <cstdint>
#include <cstdlib>

namespace mem_arena_handler
{

constexpr uint8_t ARENA_DS_BITS = 12;
constexpr uint8_t FREE_BLOCKS_DS_BITS = 20;

enum class ErrorCode : uint8_t
{
	Success = 0,
	OutOfMemory = 1,
	InsufficientResource = 2
};

struct MemoryArena
{
	~MemoryArena();

	int8_t* mem_block = nullptr;
	int8_t* untouched_mem = nullptr;
	size_t size = 0;
};

struct FreeBlock
{
	void* ptr = nullptr;
	size_t size = 0;
};

struct HandlerDataStructureInfo
{
	uint64_t arenas_len : ARENA_DS_BITS;
	uint64_t arenas_capacity : ARENA_DS_BITS;
	uint64_t free_blocks_len : FREE_BLOCKS_DS_BITS;
	uint64_t free_blocks_capacity : FREE_BLOCKS_DS_BITS;
};

struct ArenaHandler
{
	~ArenaHandler();

	[[nodiscard]]
	void* request_memory(const size_t size, const uint8_t alignment,
		const bool use_default_allocation = true);

	[[nodiscard]]
	ErrorCode free_memory(void* ptr, const size_t size);

	HandlerDataStructureInfo ds_info = {};
	MemoryArena* arenas = nullptr;
	FreeBlock* free_blocks = nullptr;
};

} // namespace mem_arena_handler

#endif // MEMORY_ARENA_HANDLER_HPP
