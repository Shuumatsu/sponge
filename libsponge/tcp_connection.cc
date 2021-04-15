#include "tcp_connection.hh"

#include <assert.h>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::send_segment(TCPSegment segment) {
    _sender.send_segment(segment);
    flush();
}

void TCPConnection::segment_received(const TCPSegment &received_segment) {
    cout << "[TCPConnection::segment_received] " << received_segment << endl;
    TCPHeader received_header = received_segment.header();

    cout << "[TCPConnection::segment_received] _connection_status: " << _connection_status << endl;

    switch (_connection_status) {
        case CLOSED: {
            // do nothing
            return;
        }
        case SYN_SENT: {
            // three-way handshake
            if (received_header.syn && received_header.ack && received_header.ackno == _sender.next_seqno()) {
                TCPSegment segment;
                segment.header().ack = true;
                segment.header().ackno = received_header.seqno + received_segment.length_in_sequence_space();
                send_segment(segment);

                _connection_status = ESTABLISHED;
                _sender.ack_received(received_header.ackno, received_header.win);
                return;
            }
            // simultaneous open
            if (received_header.syn && !received_header.ack) {
                TCPSegment segment;
                segment.header().syn = true;
                segment.header().ack = true;
                segment.header().ackno = received_header.seqno + 1;
                send_segment(segment);

                _connection_status = SYN_RCVD;
                return;
            }

            return;
        }
        case SYN_RCVD: {
            // simultaneous open
            if (received_header.ack) {
                _connection_status = ESTABLISHED;
            }
            return;
        }
        case ESTABLISHED: {
            if (received_header.rst) {
                _sender.stream_in().set_error();
                _receiver.stream_out().set_error();
                return;
            }

            if (received_header.ack) {
                _sender.ack_received(received_header.ackno, received_header.win);
            }
            _receiver.segment_received(received_segment);

            return;
        }
        default: {
            // unreachable
            assert(false);
            break;
        }
    }
}

void TCPConnection::flush() {
    for (TCPSegment segment = _sender.segments_out().front(); !_sender.segments_out().empty();
         _sender.segments_out().pop()) {
        _segments_out.push(segment);
    }
}

size_t TCPConnection::write(const string &data) {
    assert(_connection_status == ESTABLISHED);

    size_t ret = _sender.stream_in().write(data);
    _sender.fill_window();
    flush();

    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        shut_down(true);
    }
}

void TCPConnection::end_input_stream() {}

void TCPConnection::connect() {
    assert(_connection_status == CLOSED);

    TCPSegment segment;
    segment.header().syn = true;
    send_segment(segment);

    _connection_status = SYN_SENT;
}

bool TCPConnection::active() const { return _connection_status != CLOSED; }

void TCPConnection::shut_down(bool tell_remote) {
    _connection_status = CLOSED;
    if (tell_remote) {
        TCPSegment segment;
        segment.header().rst = true;
        send_segment(segment);
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
