#include "memory_arena_handler.hpp"

#include <cstdio>
#include <cstring>

namespace mem_arena_handler
{

constexpr uint16_t ARENAS_MAX_CAPACITY = (1 << ARENA_DS_BITS) - 1;
constexpr size_t DEFAULT_MEMORY_ARENA_ALLOCATION = 1 << 20;
constexpr uint32_t FREE_BLOCKS_MAX_CAPACITY = (1 << FREE_BLOCKS_DS_BITS) - 1;
constexpr uint8_t INITIAL_MEMORY_ARENAS_CAPACITY = 3;
constexpr uint8_t INITIAL_FREE_BLOCKS_CAPACITY = 50;
constexpr uint32_t MIN_FREE_BLOCK_SIZE = 256;

MemoryArena::~MemoryArena()
{
	free(mem_block);
}

ArenaHandler::~ArenaHandler()
{
	for (uint16_t ii = 0; ii < ds_info.arenas_len; ii++)
	{
		arenas[ii].~MemoryArena();
	}

	free(arenas);
	free(free_blocks);
}

static inline ErrorCode resize_arenas(ArenaHandler& handler)
{
	if (handler.ds_info.arenas_capacity == ARENAS_MAX_CAPACITY)
	{
		return ErrorCode::InsufficientResource;
	}

	if (handler.arenas == nullptr)
	{
		handler.arenas = (MemoryArena*)malloc(
			sizeof(MemoryArena) * INITIAL_MEMORY_ARENAS_CAPACITY);
		if (handler.arenas == nullptr)
		{
			return ErrorCode::OutOfMemory;
		}

		handler.ds_info.arenas_capacity = INITIAL_MEMORY_ARENAS_CAPACITY;
		return ErrorCode::Success;
	}

	uint16_t new_capacity = handler.ds_info.arenas_capacity * 2;
	if (new_capacity < handler.ds_info.arenas_capacity)
	{
		new_capacity = ARENAS_MAX_CAPACITY;
	}

	MemoryArena* mem =
		(MemoryArena*)realloc(handler.arenas, sizeof(MemoryArena) * new_capacity);
	if (mem == nullptr)
	{
		return ErrorCode::OutOfMemory;
	}

	handler.arenas = mem;
	handler.ds_info.arenas_capacity = new_capacity;
	return ErrorCode::Success;
}

static inline ErrorCode resize_free_blocks(ArenaHandler& handler)
{
	if (handler.ds_info.free_blocks_capacity == FREE_BLOCKS_MAX_CAPACITY)
	{
		return ErrorCode::InsufficientResource;
	}

	if (handler.free_blocks == nullptr)
	{
		handler.free_blocks =
			(FreeBlock*)malloc(sizeof(FreeBlock) * INITIAL_FREE_BLOCKS_CAPACITY);
		if (handler.free_blocks == nullptr)
		{
			return ErrorCode::OutOfMemory;
		}

		handler.ds_info.free_blocks_capacity = INITIAL_FREE_BLOCKS_CAPACITY;
		return ErrorCode::Success;
	}

	uint32_t new_capacity = handler.ds_info.free_blocks_capacity * 2;
	if (new_capacity < handler.ds_info.free_blocks_capacity)
	{
		new_capacity = FREE_BLOCKS_MAX_CAPACITY;
	}

	FreeBlock* mem =
		(FreeBlock*)realloc(handler.free_blocks, sizeof(FreeBlock) * new_capacity);
	if (mem == nullptr)
	{
		return ErrorCode::OutOfMemory;
	}

	handler.free_blocks = mem;
	handler.ds_info.free_blocks_capacity = new_capacity;
	return ErrorCode::Success;
}

/**
 * @brief Simply aligns `ptr` to the first aligned address, based on `alignment`,
 * greater than itself.
 **/
[[nodiscard]]
static inline void* align_forward(void* ptr, const uint8_t alignment)
{
	return (void*)(((uintptr_t)ptr + (uintptr_t)alignment - 1) &
		~((uintptr_t)alignment - 1));
}

[[nodiscard]]
static inline void* check_free_blocks(
	ArenaHandler& handler, const size_t size, const uint8_t alignment)
{
	for (uint32_t ii = 0; ii < handler.ds_info.free_blocks_len; ii++)
	{
		FreeBlock& free_block = handler.free_blocks[ii];

		// Align the free block's pointer.
		void* aligned_ptr = align_forward(free_block.ptr, alignment);

		// Calculate the needed end address for the requested block.
		//
		// If the needed end address is past what the block contains, continue on.
		const uintptr_t needed_end_addr = (uintptr_t)aligned_ptr + size;
		const uintptr_t actual_end_addr =
			(uintptr_t)free_block.ptr + free_block.size;
		if (needed_end_addr > actual_end_addr)
		{
			continue;
		}

		// The remaining size in the block may be unnecessary to keep stored,
		// bloating the number of free blocks.
		//
		// If it's smaller than a determined constant, just remove the block.
		// This keeps things fast, although it does leak small amounts of usable
		// memory from any arenas.
		if (actual_end_addr - needed_end_addr < MIN_FREE_BLOCK_SIZE)
		{
			// Copy over other blocks if needed.
			if (ii < handler.ds_info.free_blocks_len - 1)
			{
				memmove(&handler.free_blocks[ii], &handler.free_blocks[ii + 1],
					sizeof(FreeBlock) * (handler.ds_info.free_blocks_len - ii - 1));
			}

			handler.ds_info.free_blocks_len--;
		}

		// Otherwise, just update the free block's info.
		else
		{
			free_block.ptr = (void*)needed_end_addr;
			free_block.size = actual_end_addr - needed_end_addr;
		}

		return aligned_ptr;
	}

	return nullptr;
}

void* ArenaHandler::request_memory(const size_t size, const uint8_t alignment,
	const bool use_default_allocation /* = true */)
{
	// First check if any free blocks have available memory.
	if (void* ptr = check_free_blocks(*this, size, alignment); ptr != nullptr)
	{
		return ptr;
	}

	// Check if any arenas have available space.
	for (uint16_t ii = 0; ii < ds_info.arenas_len; ii++)
	{
		MemoryArena& arena = arenas[ii];

		// Align the arena's untouched pointer.
		void* aligned_ptr = align_forward(arena.untouched_mem, alignment);

		// Calculate the needed end address and the actual end address of the arena.
		//
		// If there's not enough space, continue.
		const uintptr_t needed_end_addr = (uintptr_t)aligned_ptr + size;
		const uintptr_t actual_end_addr = (uintptr_t)arena.mem_block + arena.size;
		if (needed_end_addr > actual_end_addr)
		{
			continue;
		}

		// Update the arena's info if data is used.
		arena.untouched_mem = (int8_t*)needed_end_addr;
		return aligned_ptr;
	}

	// A new memory arena is needed at this point.
	if (ds_info.arenas_len == ds_info.arenas_capacity)
	{
		const ErrorCode result = resize_arenas(*this);
		if (result == ErrorCode::OutOfMemory)
		{
			fprintf(stderr, "OOM error occurred in ArenaHandler.\n");
			return nullptr;
		}

		else if (result == ErrorCode::InsufficientResource)
		{
			fprintf(
				stderr, "Max number of memory arenas created for ArenaHandler.\n");
			return nullptr;
		}
	}

	MemoryArena& arena = arenas[ds_info.arenas_len];

	// Given the purpose of memory arenas is performance, allocate more than
	// requested.
	//
	// If the requested amount is smaller than the default allocation (and the
	// default allocation is desired), use the default allocation amount.
	size_t mem_amount = size * 3;
	if (use_default_allocation && mem_amount < DEFAULT_MEMORY_ARENA_ALLOCATION)
	{
		mem_amount = DEFAULT_MEMORY_ARENA_ALLOCATION;
	}

	arena.mem_block = (int8_t*)malloc(mem_amount);
	if (arena.mem_block == nullptr)
	{
		fprintf(stderr, "Failed to allocate memory in new memory arena.\n");
		return nullptr;
	}

	void* aligned_ptr = align_forward(arena.mem_block, alignment);
	arena.size = mem_amount;
	ds_info.arenas_len++;
	arena.untouched_mem = (int8_t*)((uintptr_t)aligned_ptr + size);
	return aligned_ptr;
}

ErrorCode ArenaHandler::free_memory(void* ptr, const size_t size)
{
	// Find the appropriate location in the sorted array for ptr.
	uint32_t low = 0;
	uint32_t high = ds_info.free_blocks_len;
	while (low < high)
	{
		uint32_t mid = low + ((high - low) / 2);
		if ((uintptr_t)free_blocks[mid].ptr < (uintptr_t)ptr)
		{
			low = mid + 1;
		}

		else
		{
			high = mid;
		}
	}

	const uint32_t idx = low;
	bool merge_left = false;
	if (idx > 0)
	{
		FreeBlock& left_block = free_blocks[idx - 1];
		if ((uintptr_t)left_block.ptr + left_block.size == (uintptr_t)ptr)
		{
			merge_left = true;
		}
	}

	bool merge_right = false;
	if (idx < ds_info.free_blocks_len)
	{
		FreeBlock& right_block = free_blocks[idx];
		if ((uintptr_t)ptr + size == (uintptr_t)right_block.ptr)
		{
			merge_right = true;
		}
	}

	// Case 1: -- Merge [left .. new .. right] into single block.
	if (merge_left && merge_right)
	{
		FreeBlock& left_block = free_blocks[idx - 1];
		FreeBlock& right_block = free_blocks[idx];

		left_block.size += size + right_block.size;
		if (idx < ds_info.free_blocks_len - 1)
		{
			memmove(&free_blocks[idx], &free_blocks[idx + 1],
				sizeof(FreeBlock) * (ds_info.free_blocks_len - (idx + 1)));
		}

		ds_info.free_blocks_len--;
		return ErrorCode::Success;
	}

	// Case 2: -- Merge [left .. new] into single block.
	if (merge_left)
	{
		free_blocks[idx - 1].size += size;
		return ErrorCode::Success;
	}

	// Case 3: -- Merge [new .. right] into single block.
	if (merge_right)
	{
		FreeBlock& right_block = free_blocks[idx];
		right_block.ptr = ptr;
		right_block.size += size;
		return ErrorCode::Success;
	}

	// Case 4: Place new block in sorted free blocks array.
	if (ds_info.free_blocks_len == ds_info.free_blocks_capacity)
	{
		const ErrorCode result = resize_free_blocks(*this);
		if (result == ErrorCode::OutOfMemory)
		{
			fprintf(stderr, "Failed to allocate memory for free blocks list.\n");
			return result;
		}

		else if (result == ErrorCode::InsufficientResource)
		{
			fprintf(stderr, "Max number of free blocks reached in ArenaHandler.\n");
			return result;
		}
	}

	if (idx < ds_info.free_blocks_len)
	{
		memmove(&free_blocks[idx + 1], &free_blocks[idx],
			sizeof(FreeBlock) * (ds_info.free_blocks_len - idx));
	}

	FreeBlock& free_block = free_blocks[idx];
	free_block.ptr = ptr;
	free_block.size = size;
	ds_info.free_blocks_len++;
	return ErrorCode::Success;
}

} // namespace mem_arena_handler
