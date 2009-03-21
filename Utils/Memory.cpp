/*
 *  Copyright 2009 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#ifdef HAVE_MALLINFO
#include <malloc.h>
#else
#ifdef HAVE_MALLOC_TRIM
#include <malloc.h>
#endif
#endif

#include "Memory.h"

#ifdef HAVE_BOOST_POOL_POOLFWD_HPP
#ifdef HAVE_UMEM_H
// Memory pool with umem
typedef fixed_singleton_pool<umem_allocator_tag,
	sizeof(char), umem_allocator,
	boost::details::pool::default_mutex, 131072> filter_pool;
#else
// Memory pool with malloc (glibc's or any other implementation)
// The tag is actually ignored
typedef fixed_singleton_pool<fixed_pool_allocator_tag,
	sizeof(char), boost::default_user_allocator_malloc_free,
	boost::details::pool::default_mutex, 131072> filter_pool;
#endif
#endif

using std::string;

Memory::Memory()
{
}

char *Memory::allocateBuffer(unsigned int length)
{
#ifdef HAVE_BOOST_POOL_POOLFWD_HPP
	return static_cast<char*>(filter_pool::ordered_malloc(length));
#else
#ifdef HAVE_UMEM_H
	return static_cast<char*>(umem_alloc(sizeof(char) * length, UMEM_DEFAULT));
#else
	return static_cast<char*>(malloc(sizeof(char) * length));
#endif
#endif
}

void Memory::freeBuffer(char *pBuffer, unsigned int length)
{
#ifdef HAVE_BOOST_POOL_POOLFWD_HPP
	filter_pool::ordered_free(static_cast<void*>(pBuffer), length);
#else
#ifdef HAVE_UMEM_H
	umem_free(static_cast<void*>(pBuffer), n * sizeof(char));
#else
	free(pBuffer);
#endif
#endif
}

void Memory::reclaim(void)
{
#ifdef HAVE_BOOST_POOL_POOLFWD_HPP
#ifdef HAVE_UMEM_H
	bool releasedMemory = fixed_singleton_pool<fixed_pool_allocator_tag, sizeof(char), umem_allocator, boost::details::pool::default_mutex, 131072>::release_memory();
#ifdef DEBUG
	std::cout << "Memory::reclaim: released umem allocated memory pool (" << releasedMemory << ")" << std::endl;
#endif
#else
	bool releasedMemory = fixed_singleton_pool<fixed_pool_allocator_tag, sizeof(char), boost::default_user_allocator_malloc_free, boost::details::pool::default_mutex, 131072>::release_memory();
#ifdef DEBUG
	std::cout << "Memory::reclaim: released malloc allocated memory pool (" << releasedMemory << ")" << std::endl;
#endif
#endif
#endif

#ifndef HAVE_UMEM_H
	// All this only makes sense if umem isn't in use
#ifdef HAVE_MALLINFO
	struct mallinfo info = mallinfo();
	/* arena: total space allocated from the heap (that is, how much the “break” point has moved since the start of the process).
	 * uordblks:number of bytes allocated and in use.
	 * fordblks:number of bytes allocated but not in use.
	 * keepcost:size of the area that can be released with a malloc_trim ().
	 * hblks:number of chunks allocated via mmap ().
	 * hblkhd:total number of bytes allocated via mmap ().
	 */
#ifdef DEBUG
	std::cout << "Memory::reclaim: allocated on the heap " << info.arena << ", mmap'ed " << info.hblkhd
		<< ", in use " << info.uordblks << ", not in use " << info.fordblks << ", can be trimmed " << info.keepcost << std::endl;
#endif
#endif
#ifdef HAVE_MALLOC_TRIM
	int trimmedMemory = malloc_trim(0);
#ifdef DEBUG
	std::cout << "Memory::reclaim: trimmed malloc allocated memory (" << trimmedMemory << ")" << std::endl;
#endif
#endif
#endif
}
