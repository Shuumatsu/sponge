#include "wrapping_integers.hh"

#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

#define DIVISOR 4294967296

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// Convert absolute seqno -> seqno.
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t wrapped = (n % DIVISOR + static_cast<uint64_t>(isn.raw_value())) % DIVISOR;
    return WrappingInt32{static_cast<uint32_t>(wrapped)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
// Convert seqno -> absolute seqno
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 wrapped_checkpoint = wrap(checkpoint, isn);
    // cout << "n: " << n.raw_value() << ", checkpoint: " << checkpoint << ", wrapped_checkpoint: " <<
    // wrapped_checkpoint
    //      << endl;

    uint64_t offset_after =
        (static_cast<uint64_t>(n.raw_value()) + DIVISOR - static_cast<uint64_t>(wrapped_checkpoint.raw_value())) %
        DIVISOR;
    uint64_t offset_before =
        (static_cast<uint64_t>(wrapped_checkpoint.raw_value()) + DIVISOR - static_cast<uint64_t>(n.raw_value())) %
        DIVISOR;

    if (offset_before < offset_after && offset_before <= checkpoint) {
        return checkpoint - offset_before;
    }
    return checkpoint + offset_after;
}