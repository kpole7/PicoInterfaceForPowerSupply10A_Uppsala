/// @file ring_spsc.h
/// @brief Simple lock-free SPSC (Single-Producer Single-Consumer) ring buffer (byte elements) using C11 atomics.
/// Assumptions:
/// - BUFFER_SIZE is a power of two.
/// - Producer and consumer are single-threaded roles (SPSC).
/// - Producer can be an ISR; consumer is main context.

#ifndef RING_SPSC_H_
#define RING_SPSC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h>

typedef struct {
    uint8_t *buffer;                 // pointer to storage (byte elements)
    size_t size;                     // buffer size (must be power-of-two)
    size_t mask;                     // size - 1
    atomic_uint_fast32_t head;       // producer index (next write position)
    atomic_uint_fast32_t tail;       // consumer index (next read position)
} ring_spsc_t;

/// @brief Initialize ring buffer structure.
/// buffer_storage must point to an array of length 'size' and size must be power-of-two.
static inline void ringSpscInit(ring_spsc_t *RingPtr, uint8_t *BufferStoragePtr, size_t Size){
    RingPtr->buffer = BufferStoragePtr;
    RingPtr->size = Size;
    RingPtr->mask = Size - 1;
    atomic_init(&RingPtr->head, 0u);
    atomic_init(&RingPtr->tail, 0u);
}

/// @brief The function pushes the byte to the ring buffer
/// Producer: push a byte. Safe to call from ISR.
/// @return true on success
/// @return false if buffer is full (element dropped).
static inline bool ringSpscPush(ring_spsc_t *RingPtr, uint8_t NewCharacter){
    uint32_t Head = atomic_load_explicit(&RingPtr->head, memory_order_relaxed);
    uint32_t Next = (Head + 1u) & (uint32_t)RingPtr->mask;
    uint32_t Tail = atomic_load_explicit(&RingPtr->tail, memory_order_acquire);
    if (Next == Tail){
        // full
        return false;
    }
    RingPtr->buffer[Head & RingPtr->mask] = NewCharacter;
    // publish the new Head so consumer can see the entry
    atomic_store_explicit(&RingPtr->head, Next, memory_order_release);
    return true;
}

/// @brief The function takes a byte from the ring buffer
/// Consumer: pop a byte. Safe to call from main context.
/// @return true and stores byte in *out on success
/// @return false if buffer empty
static inline bool ringSpscPop(ring_spsc_t *RingPtr, uint8_t *OutputDataPtr){
    uint32_t Tail = atomic_load_explicit(&RingPtr->tail, memory_order_relaxed);
    uint32_t Head = atomic_load_explicit(&RingPtr->head, memory_order_acquire);
    if (Tail == Head){
        // empty
        return false;
    }
    *OutputDataPtr = RingPtr->buffer[Tail & RingPtr->mask];
    uint32_t Next = (Tail + 1u) & (uint32_t)RingPtr->mask;
    atomic_store_explicit(&RingPtr->tail, Next, memory_order_release);
    return true;
}

/// @brief The function checks if the buffer is empty
/// @return true if the buffer is empty
/// @return false if the buffer is not empty
static inline bool ringSpscIsEmpty(ring_spsc_t *RingPtr){
    uint32_t Tail = atomic_load_explicit(&RingPtr->tail, memory_order_relaxed);
    uint32_t Head = atomic_load_explicit(&RingPtr->head, memory_order_acquire);
    return Tail == Head;
}

/// @brief The function checks if the buffer is full
/// @return true if the buffer is full
/// @return false if the buffer is not full
static inline bool ringSpscIsFull(ring_spsc_t *RingPtr){
    uint32_t Head = atomic_load_explicit(&RingPtr->head, memory_order_relaxed);
    uint32_t Next = (Head + 1u) & (uint32_t)RingPtr->mask;
    uint32_t Tail = atomic_load_explicit(&RingPtr->tail, memory_order_acquire);
    return Next == Tail;
}

#endif // RING_SPSC_H_
