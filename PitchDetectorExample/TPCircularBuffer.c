//
//  TPCircularBuffer.c
//  Circular/Ring buffer implementation
//
//  Created by Michael Tyson on 10/12/2011.
//  Copyright 2011 A Tasty Pixel. All rights reserved.
//
//  MIT license:
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
//  associated documentation files (the "Software"), to deal in the Software without restriction, 
//  including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
//  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
//  subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included in all copies or substantial 
//  portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT 
//  LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "TPCircularBuffer.h"
#include <mach/mach.h>
#include <stdio.h>

#define checkResult(result,operation) (_checkResult((result),(operation),strrchr(__FILE__, '/'),__LINE__))
static inline bool _checkResult(kern_return_t result, const char *operation, const char* file, int line) {
    if ( result != ERR_SUCCESS ) {
        printf("%s:%d: %s: %s\n", file, line, operation, mach_error_string(result)); 
        return false;
    }
    return true;
}

bool TPCircularBufferInit(TPCircularBuffer *buffer, int32_t length) {
    
    buffer->length = round_page(length);    // We need whole page sizes
    buffer->count = 0;
    // Temporarily allocate twice the length, so we have the contiguous address space to
    // support a second instance of the buffer directly after
    vm_address_t bufferAddress;
    if ( !checkResult(vm_allocate(mach_task_self(), &bufferAddress, buffer->length * 2, TRUE /* (don't use the current bufferAddress value) */),
                      "Buffer allocation") ) return false;
    
    // Now replace the second half of the allocation with a virtual copy of the first half. Deallocate the second half...
    if ( !checkResult(vm_deallocate(mach_task_self(), bufferAddress + buffer->length, buffer->length),
                      "Buffer deallocation") ) return false;
    
    // Then create a memory entry that refers to the buffer
    vm_size_t entry_length = buffer->length;
    mach_port_t memoryEntry;
    if ( !checkResult(mach_make_memory_entry(mach_task_self(), &entry_length, bufferAddress, VM_PROT_READ|VM_PROT_WRITE, &memoryEntry, 0),
                      "Create memory entry") ) {
        vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
        return false;
    }
    
    // And map the memory entry to the address space immediately after the buffer
    vm_address_t virtualAddress = bufferAddress + buffer->length;
    if ( !checkResult(vm_map(mach_task_self(), &virtualAddress, buffer->length, 0, FALSE, memoryEntry, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_DEFAULT),
                      "Map buffer memory") ) {
        vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
        return false;
    }
    
    if ( virtualAddress != bufferAddress+buffer->length ) {
        printf("Couldn't map buffer memory to end of buffer\n");
        vm_deallocate(mach_task_self(), virtualAddress, buffer->length);
        vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
        return false;
    }
    
    buffer->buffer = (void*)bufferAddress;
    buffer->fillCount = 0;
    buffer->head = buffer->tail = 0;
    
    return true;
}

void TPCircularBufferCleanup(TPCircularBuffer *buffer) {
    vm_deallocate(mach_task_self(), (vm_address_t)buffer->buffer, buffer->length * 2);
    memset(buffer, 0, sizeof(TPCircularBuffer));
}

void TPCircularBufferClear(TPCircularBuffer *buffer) {
    buffer->head = buffer->tail = 0;
    buffer->fillCount = 0;
}
