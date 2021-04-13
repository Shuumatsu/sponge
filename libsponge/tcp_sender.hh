#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <deque>
#include <functional>
#include <queue>

class RetransmissionTimer {
    bool _timer_running = false;

    //! retransmission timer for the connection
    const unsigned int _initial_retransmission_timeout;
    unsigned int _retransmission_timeout;

    size_t _passed_ticks = 0;

  public:
    RetransmissionTimer(const uint16_t retx_timeout);

    bool is_running() const { return _timer_running; }

    size_t initial_retransmission_timeout() const { return _initial_retransmission_timeout; }

    const size_t &passed_ticks() const { return _passed_ticks; }
    size_t &passed_ticks() { return _passed_ticks; }

    const unsigned int &retransmission_timeout() const { return _retransmission_timeout; }
    unsigned int &retransmission_timeout() { return _retransmission_timeout; }

    void reset();

    bool tick(const size_t ms_since_last_tick);

    void start();
    void end();
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    bool _syn_sent = false;
    bool _fin_sent = false;

    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;
    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_absolute_index{0};

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    uint64_t _bytes_in_flight = 0;

    std::deque<TCPSegment> _outstanding_segments{};

    WrappingInt32 _ackno{0};
    uint64_t _ack_absolute_index{0};
    uint16_t _window_size{1};

    RetransmissionTimer _timer;

    unsigned int _consecutive_retransmissions = 0;

    void send_segment(TCPSegment seg, uint64_t absolute_index, bool resend = false);

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_absolute_index; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_absolute_index, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
