
#include "byte_stream.hh"

#include <sstream>
#include <vector>

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t cap) {
    this->capacity = cap;
    // we use a circular array
    // we preserve one slot for indicating if it's empty or full
    //     - we allow `write_position == read_position` iff it's empty
    //     - if `(write_position + 1) % (capacity + 1) == read_position`, then it's full
    this->buffer = vector<char>(cap);
}

size_t ByteStream::increase(size_t position) const { return (position + 1) % (this->capacity + 1); }
size_t ByteStream::decrease(size_t position) const { return position > 0 ? position - 1 : this->capacity; }

bool ByteStream::buffer_empty() const { return this->read_position == this->write_position; }

bool ByteStream::buffer_full() const {
    return (this->write_position + 1) % (this->capacity + 1) == this->read_position;
}

size_t ByteStream::write(const string &data) {
    if (this->closed) {
        this->_error = true;
        return 0;
    }

    size_t ret = min(data.length(), this->remaining_capacity());
    this->written_cnt += ret;

    char const *cs = data.c_str();
    for (size_t consumed = 0; consumed < ret; consumed += 1) {
        this->buffer[this->write_position] = cs[consumed];
        this->write_position = this->increase(this->write_position);
    }

    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::stringstream ret;

    size_t bound = min(len, this->buffer_size());

    size_t curr = this->read_position;
    for (size_t consumed = 0; consumed < bound; consumed += 1) {
        ret << this->buffer[curr];
        curr = this->increase(curr);
    }

    return ret.str();
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    // size_t bound = min(len, this->buffer_size());
    // this->read_cnt += bound;

    // for (size_t consumed = 0; consumed < bound; consumed += 1) {
    //     this->write_position = this->decrease(this->write_position);
    // }
    this->read(len);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::stringstream ret;

    size_t bound = min(len, this->buffer_size());
    this->read_cnt += bound;

    for (size_t consumed = 0; consumed < bound; consumed += 1) {
        ret << this->buffer[this->read_position];
        this->read_position = this->increase(this->read_position);
    }

    return ret.str();
}

size_t ByteStream::buffer_size() const {
    if (this->read_position <= this->write_position) {
        return this->write_position - this->read_position;
    } else {
        return this->write_position + (this->capacity + 1 - this->read_position);
    }
}

void ByteStream::end_input() { this->closed = true; }

bool ByteStream::input_ended() const { return this->closed; }

bool ByteStream::eof() const { return this->closed && this->buffer_empty(); }

size_t ByteStream::bytes_written() const { return this->written_cnt * sizeof(char); }

size_t ByteStream::bytes_read() const { return this->read_cnt * sizeof(char); }

size_t ByteStream::remaining_capacity() const { return this->capacity - this->buffer_size(); }
