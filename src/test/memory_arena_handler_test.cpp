#include "memory_arena_handler.hpp"

#include "gtest/gtest.h"

using namespace mem_arena_handler;

class ArenaHandlerTest : public ::testing::Test
{
protected:
	ArenaHandler handler;

	// Helper to access private/internal state if needed
	// (ArenaHandler struct members are public by default, so we access directly)
	size_t get_arena_count()
	{
		return handler.ds_info.arenas_len;
	}

	size_t get_free_block_count()
	{
		return handler.ds_info.free_blocks_len;
	}
};

TEST_F(ArenaHandlerTest, InitializationState)
{
	EXPECT_EQ(get_arena_count(), 0);
	EXPECT_EQ(get_free_block_count(), 0);
	EXPECT_EQ(handler.arenas, nullptr);
}

TEST_F(ArenaHandlerTest, BasicAllocation)
{
	size_t alloc_size = 1024;
	void* ptr = handler.request_memory(alloc_size, 8);

	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(get_arena_count(), 1);

	// Check if the arena size is correct (Should be default 1MB or alloc * 3)
	// Default is 1<<20 (1MB)
	EXPECT_GE(handler.arenas[0].size, alloc_size);
}

TEST_F(ArenaHandlerTest, AlignmentCheck)
{
	// Request allocation with specific high alignment (e.g., 64 bytes)
	size_t alloc_size = 32;
	uint8_t alignment = 64;

	void* ptr1 = handler.request_memory(alloc_size, alignment);
	ASSERT_NE(ptr1, nullptr);
	EXPECT_EQ((uintptr_t)ptr1 % alignment, 0);

	// Alloc again to ensure next block respects alignment
	void* ptr2 = handler.request_memory(alloc_size, alignment);
	ASSERT_NE(ptr2, nullptr);
	EXPECT_EQ((uintptr_t)ptr2 % alignment, 0);
}

TEST_F(ArenaHandlerTest, MultipleAllocationsInSingleArena)
{
	// This test detects the CRITICAL BUG regarding untouched_mem

	// 1. Allocate small chunk
	void* ptr1 = handler.request_memory(128, 8);
	ASSERT_NE(ptr1, nullptr);

	// 2. Allocate another small chunk
	void* ptr2 = handler.request_memory(128, 8);
	ASSERT_NE(ptr2, nullptr);

	// Should still be in the first arena if the logic is correct
	EXPECT_EQ(get_arena_count(), 1);

	// Pointers should be distinct
	EXPECT_NE(ptr1, ptr2);

	// Distance between pointers should be at least size
	uintptr_t diff = (uintptr_t)ptr2 - (uintptr_t)ptr1;
	EXPECT_GE(diff, 128);
}

TEST_F(ArenaHandlerTest, ArenaExpansion)
{
	// 1. Allocate a standard block.
	// This creates the first arena (Capacity will be roughly 1MB default, or
	// size*3).
	void* ptr1 = handler.request_memory(1024, 1);
	ASSERT_NE(ptr1, nullptr);
	EXPECT_EQ(get_arena_count(), 1);

	// 2. Request a block SO LARGE it forces a new arena.
	// We request 10MB. Since the default first arena is ~1MB, this guarantees
	// expansion.
	size_t huge_size = 10 * 1024 * 1024;
	void* ptr2 = handler.request_memory(huge_size, 1);

	ASSERT_NE(ptr2, nullptr);
	EXPECT_EQ(get_arena_count(), 2);

	// Optional: Verify the pointers are far apart (in different memory regions)
	// This confirms they aren't adjacent in the same malloc block
	uintptr_t diff = (uintptr_t)ptr2 > (uintptr_t)ptr1
		? (uintptr_t)ptr2 - (uintptr_t)ptr1
		: (uintptr_t)ptr1 - (uintptr_t)ptr2;
	EXPECT_GT(diff, 1024);
}
TEST_F(ArenaHandlerTest, FreeMemoryAndReuse)
{
	// Allocate a specific block
	size_t size = 512;
	void* ptr = handler.request_memory(size, 1);
	ASSERT_NE(ptr, nullptr);

	// Free it
	ErrorCode err = handler.free_memory(ptr, size);
	EXPECT_EQ(err, ErrorCode::Success);
	EXPECT_EQ(get_free_block_count(), 1);

	// Request same size again - should reuse the freed block
	void* ptr2 = handler.request_memory(size, 1);

	// Should get the exact same pointer back (First-Fit strategy)
	EXPECT_EQ(ptr, ptr2);

	// Free list should be empty after reuse
	EXPECT_EQ(get_free_block_count(), 0);
}

TEST_F(ArenaHandlerTest, CoalescingFreeBlocks)
{
	// Alloc A, B, C
	size_t size = 1024; // Large enough to not trigger MIN_FREE_BLOCK_SIZE deletion
	void* pA = handler.request_memory(size, 1);
	void* pB = handler.request_memory(size, 1);
	void* pC = handler.request_memory(size, 1);

	// Free A and C (gaps)
	EXPECT_EQ(handler.free_memory(pA, size), ErrorCode::Success);
	EXPECT_EQ(handler.free_memory(pC, size), ErrorCode::Success);
	EXPECT_EQ(get_free_block_count(), 2);

	// Free B (middle) - should merge with A (left) and C (right)
	EXPECT_EQ(handler.free_memory(pB, size), ErrorCode::Success);

	// Should result in 1 large free block
	EXPECT_EQ(get_free_block_count(), 1);
	EXPECT_EQ(handler.free_blocks[0].size, size * 3);
	EXPECT_EQ(handler.free_blocks[0].ptr, pA);
}

TEST_F(ArenaHandlerTest, InvalidFreeHandling)
{
	// While the current implementation doesn't strictly validate pointers belong to
	// arenas, we can test basic mechanics.

	// Test robustness: Freeing a pointer that merges left/right
	// (Covered in CoalescingFreeBlocks)
}

TEST_F(ArenaHandlerTest, FreeBlock_DiscardSmallRemainder)
{
	// MIN_FREE_BLOCK_SIZE is 256.

	// 1. Allocate a block of 1000 bytes.
	size_t alloc_size = 1000;
	void* ptr = handler.request_memory(alloc_size, 1);

	// 2. Free it. Check result to satisfy [[nodiscard]].
	EXPECT_EQ(handler.free_memory(ptr, alloc_size), ErrorCode::Success);
	EXPECT_EQ(get_free_block_count(), 1);

	// 3. Request 800 bytes.
	// Remaining space = 200 bytes.
	// Since 200 < 256 (MIN_FREE_BLOCK_SIZE), the remainder should be discarded.
	// The free block entry should be removed entirely.
	void* ptr2 = handler.request_memory(800, 1);
	EXPECT_EQ(ptr, ptr2);				  // Should reuse the same pointer
	EXPECT_EQ(get_free_block_count(), 0); // Should be empty now
}

TEST_F(ArenaHandlerTest, FreeBlock_KeepLargeRemainder)
{
	// 1. Allocate a block of 1000 bytes.
	size_t alloc_size = 1000;
	void* ptr = handler.request_memory(alloc_size, 1);

	// 2. Free it. Check result.
	EXPECT_EQ(handler.free_memory(ptr, alloc_size), ErrorCode::Success);

	// 3. Request 500 bytes.
	// Remaining space = 500 bytes.
	// Since 500 > 256, the free block should be updated, not removed.
	void* ptr2 = handler.request_memory(500, 1);
	EXPECT_EQ(ptr, ptr2);
	EXPECT_EQ(get_free_block_count(), 1);

	// Verify the remaining size of the free block (Internal inspection)
	EXPECT_EQ(handler.free_blocks[0].size, 500);
}

TEST_F(ArenaHandlerTest, FreeBlocks_ArrayResize)
{
	// INITIAL_FREE_BLOCKS_CAPACITY is 50.
	// We need to create > 50 non-contiguous free blocks to trigger a resize.

	const int num_blocks = 60;
	void* ptrs[num_blocks];
	void* padding[num_blocks];

	// Allocate patterns: [Block][Padding][Block][Padding]...
	// The padding prevents the blocks from merging when we free them.
	for (int i = 0; i < num_blocks; ++i)
	{
		ptrs[i] = handler.request_memory(100, 1);
		padding[i] = handler.request_memory(100, 1);
	}

	// Free all the main blocks.
	for (int i = 0; i < num_blocks; ++i)
	{
		// Must check return value here. Using ASSERT to stop on first failure.
		ASSERT_EQ(handler.free_memory(ptrs[i], 100), ErrorCode::Success);
	}

	// We should now have 60 individual free blocks.
	// This asserts that the vector resized successfully from 50 to 100.
	EXPECT_EQ(get_free_block_count(), num_blocks);

	// Verify logic still works by allocating one of them back
	void* new_ptr = handler.request_memory(100, 1);
	EXPECT_NE(new_ptr, nullptr);
	EXPECT_EQ(get_free_block_count(), num_blocks - 1);
}

TEST_F(ArenaHandlerTest, Arenas_ArrayResize)
{
	// INITIAL_MEMORY_ARENAS_CAPACITY is 3.
	// Each new arena is allocated with a capacity of (request_size * 3).
	// This means one arena can hold approximately 3 allocations of 'size'.

	// To trigger a resize of the arena list (capacity 3 -> 6), we need to
	// create 4 arenas. To create 5 arenas (to be safe), we need to fill 4 arenas.
	// 4 arenas * 3 allocations/arena = 12 allocations.
	// The 13th allocation will create the 5th arena.

	size_t size = 1024 * 1024; // 1MB

	// We loop 15 times to safely guarantee we exceed the capacity of the first few
	// arenas. Allocations 1-3   -> Arena 0 Allocations 4-6   -> Arena 1 Allocations
	// 7-9   -> Arena 2 Allocations 10-12 -> Arena 3 (Resize triggered here or at 10
	// depending on logic) Allocations 13-15 -> Arena 4
	for (int i = 0; i < 15; ++i)
	{
		void* ptr = handler.request_memory(size, 1);
		ASSERT_NE(ptr, nullptr);
	}

	// We expect at least 5 arenas to exist now.
	EXPECT_GE(get_arena_count(), 5);
}

TEST_F(ArenaHandlerTest, Allocation_NoDefaultFlag)
{
	// Test the 'use_default_allocation = false' path.
	// Logic: mem_amount = size * 3.

	size_t size = 1000;
	// Pass false as the 3rd argument
	void* ptr = handler.request_memory(size, 1, false);

	ASSERT_NE(ptr, nullptr);

	// The arena size should be exactly size * 3 = 3000
	// (It would be 1MB (1048576) if the flag was true)
	EXPECT_EQ(handler.arenas[0].size, 3000);
}

TEST_F(ArenaHandlerTest, Coverage_Fragmentation_AlignmentSkip)
{
	// Targets Line 137: Ensures we skip a free block if alignment requirements
	// push the start pointer too far forward to fit the size.

	// 1. Create a misaligned free block.
	// Alloc A (1 byte) -> Pushes ptr to an odd address (likely) or just offsets it.
	void* pA = handler.request_memory(1, 1);
	ASSERT_NE(pA, nullptr); // Fix unused var warning

	// Alloc B (64 bytes). This will be our target block.
	void* pB = handler.request_memory(64, 1);
	ASSERT_NE(pB, nullptr);

	// Alloc C (1 byte) -> Prevents B from being the "last" block or merging right.
	void* pC = handler.request_memory(1, 1);
	ASSERT_NE(pC, nullptr); // Fix unused var warning

	// Free B. We now have a 64-byte free block.
	// Fix nodiscard warning
	ASSERT_EQ(handler.free_memory(pB, 64), ErrorCode::Success);

	// 2. Request memory that fits in 64 bytes (size 50),
	// but requires high alignment (64) that forces padding.
	// Use a large alignment to force the skip.
	void* pNew = handler.request_memory(50, 64);
	ASSERT_NE(pNew, nullptr);

	// It should NOT reuse pB (unless pB happened to be perfectly 64-aligned
	// already).
	if (pNew != pB)
	{
		EXPECT_EQ(get_free_block_count(), 1);
		EXPECT_EQ(handler.free_blocks[0].ptr, pB);
	}
}

TEST_F(ArenaHandlerTest, Coverage_MergeLeftOnly)
{
	// Targets Lines 307-311: Merge [Left + New]

	void* pA = handler.request_memory(100, 1); // Left
	void* pB = handler.request_memory(100, 1); // New
	void* pC = handler.request_memory(100, 1); // Barrier (prevents right merge)
	ASSERT_NE(pC, nullptr);					   // Fix unused var warning

	// Free A (Left)
	EXPECT_EQ(handler.free_memory(pA, 100), ErrorCode::Success);

	// Free B. Should merge into A.
	EXPECT_EQ(handler.free_memory(pB, 100), ErrorCode::Success);

	// Verify: 1 Free block, Size 200, Ptr == pA
	EXPECT_EQ(get_free_block_count(), 1);
	EXPECT_EQ(handler.free_blocks[0].size, 200);
	EXPECT_EQ(handler.free_blocks[0].ptr, pA);
}

TEST_F(ArenaHandlerTest, Coverage_MergeRightOnly)
{
	// Targets Lines 314-320: Merge [New + Right]

	void* pA = handler.request_memory(100, 1); // Barrier (prevents left merge)
	ASSERT_NE(pA, nullptr);					   // Fix unused var warning

	void* pB = handler.request_memory(100, 1); // New
	void* pC = handler.request_memory(100, 1); // Right

	// Free C (Right)
	EXPECT_EQ(handler.free_memory(pC, 100), ErrorCode::Success);

	// Free B. Should merge into C.
	EXPECT_EQ(handler.free_memory(pB, 100), ErrorCode::Success);

	// Verify: 1 Free block, Size 200, Ptr == pB
	EXPECT_EQ(get_free_block_count(), 1);
	EXPECT_EQ(handler.free_blocks[0].size, 200);
	EXPECT_EQ(handler.free_blocks[0].ptr, pB);
}

TEST_F(ArenaHandlerTest, Coverage_MergeBoth_ShiftTail)
{
	// Targets Lines 297-300: Merge [Left + New + Right] AND shift subsequent blocks.

	void* pA = handler.request_memory(100, 1); // Left
	void* pB = handler.request_memory(100, 1); // Middle (New)
	void* pC = handler.request_memory(100, 1); // Right

	// BARRIER: Prevents pD from merging with pC.
	// We need pD to remain a separate entry in the list to verify it gets shifted.
	void* barrier = handler.request_memory(10, 1);
	ASSERT_NE(barrier, nullptr);

	void* pD = handler.request_memory(100, 1); // Tail Block (Needs to shift)

	// Free A, C, and D.
	// They are separated by B and Barrier, so no merges happen yet.
	EXPECT_EQ(handler.free_memory(pA, 100), ErrorCode::Success);
	EXPECT_EQ(handler.free_memory(pC, 100), ErrorCode::Success);
	EXPECT_EQ(handler.free_memory(pD, 100), ErrorCode::Success);

	// Current Free List state:
	// [0]: A (100)
	// [1]: C (100)
	// [2]: D (100)
	EXPECT_EQ(get_free_block_count(), 3);

	// Free B.
	// This sits between A and C. It should merge A, B, and C.
	// The list should shrink by 1, and D (at index 2) should shift down to index 1.
	EXPECT_EQ(handler.free_memory(pB, 100), ErrorCode::Success);

	// Expect 2 blocks: [A+B+C] and [D]
	EXPECT_EQ(get_free_block_count(), 2);

	// Check Block 0 (Merged A+B+C = 300 bytes)
	EXPECT_EQ(handler.free_blocks[0].ptr, pA);
	EXPECT_EQ(handler.free_blocks[0].size, 300);

	// Check Block 1 (D = 100 bytes) - confirming it shifted correctly
	EXPECT_EQ(handler.free_blocks[1].ptr, pD);
	EXPECT_EQ(handler.free_blocks[1].size, 100);
}
TEST_F(ArenaHandlerTest, Coverage_InsertMiddle)
{
	// Targets Lines 340-343: Insert a block into the middle of the array (no merge).

	void* pA = handler.request_memory(100, 1);	// Block 1
	void* pad1 = handler.request_memory(10, 1); // Padding
	ASSERT_NE(pad1, nullptr);					// Fix unused var warning

	void* pB = handler.request_memory(100, 1); // Block 2 (Middle)

	void* pad2 = handler.request_memory(10, 1); // Padding
	ASSERT_NE(pad2, nullptr);					// Fix unused var warning

	void* pC = handler.request_memory(100, 1); // Block 3

	// Free A and C. List: [A, C]
	EXPECT_EQ(handler.free_memory(pA, 100), ErrorCode::Success);
	EXPECT_EQ(handler.free_memory(pC, 100), ErrorCode::Success);

	// Free B. Should insert between A and C.
	EXPECT_EQ(handler.free_memory(pB, 100), ErrorCode::Success);

	EXPECT_EQ(get_free_block_count(), 3);
	EXPECT_EQ(handler.free_blocks[0].ptr, pA);
	EXPECT_EQ(handler.free_blocks[1].ptr, pB); // Inserted here
	EXPECT_EQ(handler.free_blocks[2].ptr, pC);
}
