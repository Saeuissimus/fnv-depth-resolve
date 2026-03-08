#include "BSMemory.hpp"
#include <windows.h>

namespace BSMemory {

	// -------------------------------------------------------------------------
	// Internal globals and functions
	// -------------------------------------------------------------------------
	static INIT_ONCE	allocInitOnce = INIT_ONCE_STATIC_INIT;
	void*				pMemoryManager = nullptr;
	void* __fastcall	InitAllocate(void* apThis, void*, std::size_t size);
	void* __fastcall	InitReAllocate(void* apThis, void*, void* ptr, std::size_t new_size);
	void __fastcall		InitDeallocate(void* apThis, void*, void* ptr);
	size_t __fastcall	InitSize(void* apThis, void*, void* ptr);

	static BOOL CALLBACK BSAllocatorInitializer(PINIT_ONCE, PVOID, PVOID*);

	namespace CurrentMemManager {
		void*	(__thiscall* Allocate)(void* apThis, std::size_t size) = (void* (__thiscall*)(void*, std::size_t))InitAllocate;
		void*	(__thiscall* ReAllocate)(void* apThis, void* ptr, std::size_t new_size) = (void* (__thiscall*)(void*, void*, std::size_t))InitReAllocate;
		void	(__thiscall* Deallocate)(void* apThis, void* ptr) = (void(__thiscall*)(void*, void*))InitDeallocate;
		size_t	(__thiscall* Size)(void* apThis, void* ptr) = (size_t (__thiscall*)(void*, void*))InitSize;
	}

	// -------------------------------------------------------------------------
	// Functions made to be used by the user
	// These functions use game's memory manager to manage memory
	// They are used to replace new and delete operators
	// -------------------------------------------------------------------------
	__declspec(allocator) __declspec(restrict) void* BSNew(std::size_t size) {
		return CurrentMemManager::Allocate(pMemoryManager, size);
	}

	__declspec(allocator) __declspec(restrict) void* BSReallocate(void* ptr, std::size_t new_size) {
		return CurrentMemManager::ReAllocate(pMemoryManager, ptr, new_size);
	}

	__declspec(noalias) void BSFree(void* ptr) {
		CurrentMemManager::Deallocate(pMemoryManager, ptr);
	}

	__declspec(noalias) std::size_t BSSize(void* ptr) {
		return CurrentMemManager::Size(pMemoryManager, ptr);
	}

	// -------------------------------------------------------------------------
	// Functions made to initialize the allocator
	// Compatible with both game and GECK
	// -------------------------------------------------------------------------

	// This function is used to create game's heap if it doesn't exist
	// It's possible to load the plugin before game is even initialized
	// In those cases, malloc fails due to lack of heap - that's why we need to create it manually
	__declspec(noinline) void __fastcall CreateHeapIfNotExisting(uint32_t auiCreateHeapAddress, uint32_t auiHeapAddress, uint32_t auiCallAddress, uint32_t auiJumpAddress) {
		if (*(HANDLE*)auiHeapAddress)
			return;
		
		auto CreateHeap = (HANDLE(__cdecl*)(uint32_t))auiCreateHeapAddress;
		CreateHeap(true);

		auto PatchMemoryNop = [](uint32_t address, size_t size) {
			DWORD d = 0;
			VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &d);

			for (SIZE_T i = 0; i < size; i++)
				*(volatile BYTE*)(address + i) = 0x90;

			VirtualProtect((LPVOID)address, size, d, &d);

			FlushInstructionCache(GetCurrentProcess(), (LPVOID)address, size);
		};

		auto SafeWrite8 = [](SIZE_T addr, SIZE_T data) {
			SIZE_T	oldProtect;

			VirtualProtect((void*)addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
			*((uint8_t*)addr) = data;
			VirtualProtect((void*)addr, 4, oldProtect, &oldProtect);
		};

		PatchMemoryNop(auiCallAddress, 5);
		SafeWrite8(auiJumpAddress, 0xEB);
	}

	static BOOL EnsureAllocatorInitialized() noexcept {
		return InitOnceExecuteOnce(&allocInitOnce, BSAllocatorInitializer, nullptr, nullptr);
	}

	// This function sets up correct addresses based on the program
	static BOOL CALLBACK BSAllocatorInitializer(PINIT_ONCE, PVOID, PVOID*) {
		if (*reinterpret_cast<uint8_t*>(0x401190) != 0x55) {
			// Is GECK
			CreateHeapIfNotExisting(0xC770C3, 0xF9907C, 0xC62B21, 0xC62B29);
			pMemoryManager = reinterpret_cast<void*>(0xF21B5C);
			CurrentMemManager::Allocate = (void* (__thiscall*)(void*, size_t))0x8540A0;
			CurrentMemManager::ReAllocate = (void* (__thiscall*)(void*, void*, size_t))0x8543B0;
			CurrentMemManager::Deallocate = (void(__thiscall*)(void*, void*))0x8542C0;
			CurrentMemManager::Size = (size_t(__thiscall*)(void*, void*))0x854720;
		}
		else {
			CreateHeapIfNotExisting(0xEDDB6A, 0x12705BC, 0xECC3CB, 0xECC3D3);
			pMemoryManager = reinterpret_cast<void*>(0x11F6238);
			CurrentMemManager::Allocate = (void* (__thiscall*)(void*, size_t))0xAA3E40;
			CurrentMemManager::ReAllocate = (void* (__thiscall*)(void*, void*, size_t))0xAA4150;
			CurrentMemManager::Deallocate = (void(__thiscall*)(void*, void*))0xAA4060;
			CurrentMemManager::Size = (size_t(__thiscall*)(void*, void*))0xAA44C0;
		}

		return TRUE;
	}

	void* __fastcall InitAllocate(void* apThis, void*, std::size_t size) {
		if (EnsureAllocatorInitialized())
			return CurrentMemManager::Allocate(pMemoryManager, size);
		return nullptr;
	}

	void* __fastcall InitReAllocate(void* apThis, void*, void* ptr, std::size_t new_size) {
		if (EnsureAllocatorInitialized())
			return CurrentMemManager::ReAllocate(pMemoryManager, ptr, new_size);
		return nullptr;
	}

	void __fastcall InitDeallocate(void* apThis, void*, void* ptr) {
		EnsureAllocatorInitialized();
		CurrentMemManager::Deallocate(pMemoryManager, ptr);
	}

	size_t __fastcall InitSize(void* apThis, void*, void* ptr) {
		EnsureAllocatorInitialized();
		return CurrentMemManager::Size(pMemoryManager, ptr);
	}
}