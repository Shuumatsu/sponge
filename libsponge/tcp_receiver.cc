#include "tcp_receiver.hh"

#include <assert.h>
#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    struct TCPHeader header = seg.header();
    WrappingInt32 data_seqno = header.syn ? header.seqno + 1 : header.seqno;

    if (!_syn_received && !header.syn) {
        return;
    }

    if (header.syn) {
        if (_syn_received) {
            assert(_isn_seqno == header.seqno);
        }

        _syn_received = true;
        _isn_seqno = header.seqno;
        cout << "_isn_seqno: " << _isn_seqno << endl;
    }
    if (header.fin) {
        WrappingInt32 fin_seqno = data_seqno + seg.payload().size();

        uint64_t fin_absolute_index = unwrap(fin_seqno, _isn_seqno, _checkpoint);
        // cout << "_fin_seqno: " << _fin_seqno << ", _isn_seqno: " << _isn_seqno << ", _checkpoint: " << _checkpoint
        //      << endl;
        if (_fin_received) {
            assert(_fin_absolute_index == fin_absolute_index);
        }

        _fin_received = true;
        _fin_seqno = fin_seqno;
        _fin_absolute_index = fin_absolute_index;

        cout << "_fin_seqno: " << _fin_seqno << ", _fin_absolute_index: " << _fin_absolute_index << endl;
    }

    uint64_t absolute_index = unwrap(data_seqno, _isn_seqno, _checkpoint);
    _checkpoint = absolute_index;

    uint64_t stream_index = absolute_index - 1;
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_received) {
        return std::nullopt;
    }

    uint64_t stream_index = _reassembler.waiting_index();
    cout << "stream_index: " << stream_index << endl;
    uint64_t absolute_index = stream_index + 1;

    WrappingInt32 ret = wrap(absolute_index, _isn_seqno);
    if (_fin_received && absolute_index == _fin_absolute_index) {
        return ret + 1;
    }
    return ret;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
