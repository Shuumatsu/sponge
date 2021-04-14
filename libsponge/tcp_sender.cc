#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <assert.h>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

RetransmissionTimer::RetransmissionTimer(const uint16_t retx_timeout)
    : _initial_retransmission_timeout{retx_timeout}, _retransmission_timeout{retx_timeout} {}

void RetransmissionTimer::reset() {
    _passed_ticks = 0;
    _retransmission_timeout = _initial_retransmission_timeout;
}

void RetransmissionTimer::start() {
    _timer_running = true;
    _passed_ticks = 0;
}

void RetransmissionTimer::end() { _timer_running = false; }

bool RetransmissionTimer::tick(const size_t ms_since_last_tick) {
    if (_timer_running) {
        _passed_ticks += ms_since_last_tick;
    }

    if (_timer_running && _passed_ticks >= _retransmission_timeout) {
        return true;
    }
    return false;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
// responsible for reading from a ByteStream and turning the stream into a sequence of outgoing TCP segments.
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()})), _stream(capacity), _timer(retx_timeout) {}

// send_segment(seg); resend_segment(seg, absolute_index, true)
void TCPSender::send_segment(TCPSegment segment, uint64_t absolute_index, bool resend) {
    segment.header().seqno = wrap(absolute_index, _isn);

    if (resend) {
        _outstanding_segments.push_front(segment);
    } else {
        _outstanding_segments.push_back(segment);
    }
    _segments_out.push(segment);

    cout << "[TCPSender::send_segment] " << segment;
    cout << "; [" << absolute_index << ", " << absolute_index + segment.length_in_sequence_space() << ")" << endl;

    _syn_sent |= segment.header().syn;
    _fin_sent |= segment.header().fin;

    _bytes_in_flight += segment.length_in_sequence_space();
    _next_absolute_index = max(_next_absolute_index, absolute_index + segment.length_in_sequence_space());

    // 4. Every time a segment containing data (nonzero length in sequence space) is sent
    // (whether it’s the first time or a retransmission), if the timer is not running, start it
    if (segment.length_in_sequence_space() != 0 && !_timer.is_running()) {
        _timer.start();
    }
}

void TCPSender::send_segment(TCPSegment segment) { send_segment(segment, _next_absolute_index); }

void TCPSender::send_empty_segment() { send_segment(TCPSegment{}); }

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // I don't think this should be processed inside TCPSender
    if (!_syn_sent) {
        TCPSegment seg;
        seg.header().syn = true;

        send_segment(seg);
        return;
    }

    assert(_next_absolute_index >= _ack_absolute_index);

    uint64_t first_unacceptable_absolute_index =
        _ack_absolute_index + static_cast<uint64_t>((_window_size == 0 ? 1 : _window_size));

    cout << "[fill_window] _ack_absolute_index: " << _ack_absolute_index << ", _window_size: " << _window_size << endl;
    cout << "[fill_window] _next_absolute_index: " << _next_absolute_index
         << ", first_unacceptable_absolute_index: " << first_unacceptable_absolute_index << endl;

    while (_next_absolute_index < first_unacceptable_absolute_index && !_fin_sent) {
        size_t window_limitation = first_unacceptable_absolute_index - _next_absolute_index;
        size_t size = min(TCPConfig::MAX_PAYLOAD_SIZE, window_limitation);

        string str = _stream.read(size);
        cout << "[fill_window] size: " << size << ", str.size: " << str.size() << endl;

        TCPSegment seg;
        seg.payload() = Buffer(std::move(str));

        if (_stream.eof() && seg.length_in_sequence_space() < window_limitation) {
            seg.header().fin = true;
        }

        // what if str is empty and eof is true??
        // in this case fill_window does nothing, is this ok?

        if (seg.length_in_sequence_space() == 0) {
            break;
        }

        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t prev_ack_absolute_index = _ack_absolute_index;

    uint64_t ack_absolute_index = unwrap(ackno, _isn, _ack_absolute_index);
    if (ack_absolute_index < prev_ack_absolute_index || ack_absolute_index > _next_absolute_index) {
        return;
    }
    _ackno = ackno;
    _ack_absolute_index = ack_absolute_index;
    _window_size = window_size;

    cout << "[ack_received] ackno: " << ackno << ", ack_absolute_index: " << ack_absolute_index
         << ", window_size: " << window_size << endl;

    cout << "[ack_received] _outstanding_segments.size(before filter): " << _outstanding_segments.size() << endl;
    for (; !_outstanding_segments.empty(); _outstanding_segments.pop_front()) {
        TCPSegment seg = _outstanding_segments.front();

        uint64_t absolute_index = unwrap(seg.header().seqno, _isn, ack_absolute_index);
        if (absolute_index < _ack_absolute_index &&
            _ack_absolute_index - absolute_index >= seg.length_in_sequence_space()) {
            _bytes_in_flight -= seg.length_in_sequence_space();

        } else {
            break;
        }
    }
    cout << "[ack_received] _outstanding_segments.size(after filter): " << _outstanding_segments.size() << endl;

    // 7. When the receiver gives the sender an ackno that acknowledges the successful receipt of new data
    if (_ack_absolute_index > prev_ack_absolute_index) {
        _timer.retransmission_timeout() = _timer.initial_retransmission_timeout();
        if (_outstanding_segments.size() > 0) {
            _timer.passed_ticks() = 0;
        }
        _consecutive_retransmissions = 0;
    }

    cout << "[ack_received] _ackno: " << _ackno << ", _ack_absolute_index: " << _ack_absolute_index
         << ", window_size: " << _window_size << endl;

    // 5. When all outstanding data has been acknowledged, stop the retransmission timer
    if (_outstanding_segments.size() == 0) {
        _timer.end();
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    cout << "[tick | before]  is_running: " << _timer.is_running() << ", passed_ticks: " << _timer.passed_ticks()
         << ", retransmission_timeout: " << _timer.retransmission_timeout() << endl;
    cout << "[tick | after] _outstanding_segments.size(after filter): " << _outstanding_segments.size() << endl;

    // 6. If tick is called and the retransmission timer has expired:
    if (_timer.tick(ms_since_last_tick) && _outstanding_segments.size() > 0) {
        TCPSegment seg = _outstanding_segments.front();
        _outstanding_segments.pop_front();
        _bytes_in_flight -= seg.length_in_sequence_space();

        uint64_t absolute_index = unwrap(seg.header().seqno, _isn, _ack_absolute_index);
        send_segment(seg, absolute_index, true);

        if (_window_size != 0) {
            _consecutive_retransmissions += 1;
            _timer.retransmission_timeout() *= 2;
        }
        _timer.passed_ticks() = 0;
    }

    if (_outstanding_segments.size() == 0) {
        _timer.end();
    }

    cout << "[tick | after] _outstanding_segments.size(after filter): " << _outstanding_segments.size() << endl;
    cout << "[tick | after] is_running: " << _timer.is_running() << ", passed_ticks: " << _timer.passed_ticks()
         << ", retransmission_timeout: " << _timer.retransmission_timeout() << endl;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }
