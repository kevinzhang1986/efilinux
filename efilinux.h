/*
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * This file contains some wrappers around the gnu-efi functions. As
 * we're not going through uefi_call_wrapper() directly, this allows
 * us to get some type-safety for function call arguments and for the
 * compiler to check that the number of function call arguments is
 * correct.
 *
 * It's also a good place to document the EFI interface.
 */

#ifndef __EFILINUX_H__
#define __EFILINUX_H__

static EFI_SYSTEM_TABLE *sys_table;
static EFI_BOOT_SERVICES *boot;
static EFI_RUNTIME_SERVICES *runtime;

/**
 * register_table - Register the EFI system table
 * @_table: EFI system table
 *
 * If the system table CRC check fails return FALSE, otherwise return
 * TRUE.
 */
static inline BOOLEAN register_table(EFI_SYSTEM_TABLE *_table)
{
	sys_table = _table;
	boot = sys_table->BootServices;
	runtime = sys_table->RuntimeServices;

	return CheckCrc(sys_table->Hdr.HeaderSize, sys_table);
}

/**
 * allocate_pages - Allocate memory pages from the system
 * @atype: type of allocation to perform
 * @mtype: type of memory to allocate
 * @num_pages: number of contiguous 4KB pages to allocate
 * @memory: used to return the address of allocated pages
 *
 * Allocate @num_pages physically contiguous pages from the system
 * memory and return a pointer to the base of the allocation in
 * @memory if the allocation succeeds. On success, the firmware memory
 * map is updated accordingly.
 */
static inline EFI_STATUS
allocate_pages(EFI_ALLOCATE_TYPE atype, EFI_MEMORY_TYPE mtype,
	       UINTN num_pages, EFI_PHYSICAL_ADDRESS *memory)
{
	return uefi_call_wrapper(boot->AllocatePages, 4, atype,
				 mtype, num_pages, memory);
}

/**
 * free_pages - Return memory allocated by allocate_pages() to the firmware
 * @memory: physical base address of the page range to be freed
 * @num_pages: number of contiguous 4KB pages to free
 *
 * On success, the firmware memory map is updated accordingly.
 */
static inline EFI_STATUS
free_pages(EFI_PHYSICAL_ADDRESS memory, UINTN num_pages)
{
	return uefi_call_wrapper(boot->FreePages, 2, memory, num_pages);
}

/**
 * allocate_pool - Allocate pool memory
 * @type: the type of pool to allocate
 * @size: number of bytes to allocate from pool of @type
 * @buffer: used to return the address of allocated memory
 *
 * Allocate memory from pool of @type. If the pool needs more memory
 * pages are allocated from EfiConventionalMemory in order to grow the
 * pool.
 *
 * All allocations are eight-byte aligned.
 */
static inline EFI_STATUS
allocate_pool(EFI_MEMORY_TYPE type, UINTN size, void **buffer)
{
	return uefi_call_wrapper(boot->AllocatePool, 3, type, size, buffer);
}

/**
 * free_pool - Return pool memory to the system
 * @buffer: the buffer to free
 *
 * Return @buffer to the system. The returned memory is marked as
 * EfiConventionalMemory.
 */
static inline EFI_STATUS free_pool(void *buffer)
{
	return uefi_call_wrapper(boot->FreePool, 1, buffer);
}

/**
 * get_memory_map - Return the current memory map
 * @size: the size in bytes of @map
 * @map: buffer to hold the current memory map
 * @key: used to return the key for the current memory map
 * @descr_size: used to return the size in bytes of EFI_MEMORY_DESCRIPTOR
 * @descr_version: used to return the version of EFI_MEMORY_DESCRIPTOR
 *
 * Get a copy of the current memory map. The memory map is an array of
 * EFI_MEMORY_DESCRIPTORs. An EFI_MEMORY_DESCRIPTOR describes a
 * contiguous block of memory.
 *
 * On success, @key is updated to contain an identifer for the current
 * memory map. The firmware's key is changed every time something in
 * the memory map changes. @size is updated to indicate the size of
 * the memory map pointed to by @map.
 *
 * @descr_size and @descr_version are used to ensure backwards
 * compatibility with future changes made to the EFI_MEMORY_DESCRIPTOR
 * structure. @descr_size MUST be used when the size of an
 * EFI_MEMORY_DESCRIPTOR is used in a calculation, e.g when iterating
 * over an array of EFI_MEMORY_DESCRIPTORs.
 *
 * On failure, and if the buffer pointed to by @map is too small to
 * hold the memory map, EFI_BUFFER_TOO_SMALL is returned and @size is
 * updated to reflect the size of a buffer required to hold the memory
 * map.
 */
static inline EFI_STATUS
get_memory_map(UINTN *size, EFI_MEMORY_DESCRIPTOR *map, UINTN *key,
	       UINTN *descr_size, UINTN *descr_version)
{
	return uefi_call_wrapper(boot->GetMemoryMap, 5, size, map,
				 key, descr_size, descr_version);
}

/**
 * exit_boot_serivces - 
 */
static inline EFI_STATUS
exit_boot_services(EFI_HANDLE image, UINTN key)
{
	return uefi_call_wrapper(boot->ExitBootServices, 2, image, key);
}

/**
 * exit - Terminate a loaded EFI image
 * @image: firmware-allocated handle that identifies the image
 * @status: the image's exit code
 * @size: size in bytes of @data. Ignored if @status is EFI_SUCCESS
 * @reason: a NUL-terminated status string, optionally followed by binary data
 *
 * This function terminates @image and returns control to the boot
 * services. This function MUST NOT be called until all loaded child
 * images have exited. All memory allocated by the image must be freed
 * before calling this function, apart from the buffer @reason, which
 * will be freed by the firmware.
 */
static inline EFI_STATUS
exit(EFI_HANDLE image, EFI_STATUS status, UINTN size, CHAR16 *reason)
{
	return uefi_call_wrapper(boot->Exit, 4, image, status, size, reason);
}

#define PAGE_SIZE	4096

/*
 * Convert bytes into pages
 */
static inline UINTN num_pages(UINTN size)
{
	UINTN pages;

	pages = ((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
	return (pages /= PAGE_SIZE);
}

static const CHAR16 *memory_types[] = {
	L"EfiReservedMemoryType",
	L"EfiLoaderCode",
	L"EfiLoaderData",
	L"EfiBootServicesCode",
	L"EfiBootServicesData",
	L"EfiRuntimeServicesCode",
	L"EfiRuntimeServicesData",
	L"EfiConventionalMemory",
	L"EfiUnusableMemory",
	L"EfiACPIReclaimMemory",
	L"EfiACPIMemoryNVS",
	L"EfiMemoryMappedIO",
	L"EfiMemoryMappedIOPortSpace",
	L"EfiPalCode",
};

static inline const CHAR16 *memory_type_to_str(UINT32 type)
{
	if (type > sizeof(memory_types)/sizeof(CHAR16 *))
		return L"Unknown";

	return memory_types[type];
}

#endif /* __EFILINUX_H__ */
