#include "stream_reassembler.hh"

#include <assert.h>
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

// - Is it okay for our re-assembly data structure to store overlapping substrings? No.
// So we will need to merge every blocks

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

void StreamReassembler::_insert_block(struct block incomming) {
    // find greatest not greater, return `size()` if not exist
    function<int(int, int)> f = [&](int left, int right) {
        if (left + 1 >= right) {
            if (_blocks[right].index <= incomming.index) {
                return right;
            }
            if (_blocks[left].index <= incomming.index) {
                return left;
            }
            return -1;
        }

        int mid = (left + right) / 2;
        if (_blocks[mid].index <= incomming.index) {
            return f(mid, right);
        }
        return f(left, mid - 1);
    };

    int pos = _blocks.empty() ? -1 : f(0, _blocks.size() - 1);
    cout << "pos: " << pos << endl;
    deque<struct block> stack(_blocks.begin(), _blocks.begin() + pos + 1);
    if (stack.empty() || incomming.index > stack.back().index + stack.back().data.size()) {
        stack.push_back(incomming);
    } else {
        struct block prev = stack.back();
        stack.pop_back();

        struct block after {
            prev.index, prev.data.substr(0, incomming.index - prev.index) + incomming.data +
                            (incomming.index + incomming.data.size() < prev.index + prev.data.size()
                                 ? prev.data.substr(incomming.index - prev.index + incomming.data.size())
                                 : "")
        };

        stack.push_back(after);
    }

    for (int i = pos + 1; i < int(_blocks.size()); i += 1) {
        struct block curr = _blocks[i];

        struct block prev = stack.back();
        if (curr.index > prev.index + prev.data.size()) {
            stack.push_back(curr);
            continue;
        }
        stack.pop_back();

        int offset = (prev.index + prev.data.size()) - curr.index;
        struct block after {
            prev.index, prev.data + (offset <= int(curr.data.size()) ? curr.data.substr(offset) : "")
        };

        stack.push_back(after);
    }

    _blocks = stack;

    _unassembled_bytes = 0;
    for (struct block _block : _blocks) {
        _unassembled_bytes += _block.data.size();
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    cout << endl;
    function<void()> finish = [&]() {
        _eof |= eof;
        if (_eof && this->empty()) {
            _output.end_input();
        }
    };

    if (index > _waiting_index + _capacity || data.size() == 0 || index + data.size() <= _waiting_index) {
        finish();
        return;
    }

    struct block incomming {
        max(_waiting_index, index), index < _waiting_index ? data.substr(_waiting_index - index) : data.substr(0)
    };
    std::cout << "eof: " << eof << ", incomming_index: " << incomming.index
              << ", incomming_size: " << incomming.data.size() << std::endl;

    cout << "before insertion: _blocks.size() = " << _blocks.size() << ", _unassembled_bytes = " << _unassembled_bytes
         << endl;
    for (auto _block : _blocks) {
        std::cout << "block_index: " << _block.index << ", block_size: " << _block.data.size() << std::endl;
    }

    _insert_block(incomming);
    cout << "before transition: _blocks.size() = " << _blocks.size() << ", _unassembled_bytes = " << _unassembled_bytes
         << endl;
    for (auto _block : _blocks) {
        std::cout << "    block_index: " << _block.index << ", block_size: " << _block.data.size() << std::endl;
        // cout << "    ";
        // for (size_t i = 0; i < _block.data.size(); i += 1) {
        //     printf("%d ", _block.data.c_str()[i]);
        // }
        // cout << endl;
    }

    if (_blocks[0].index <= _waiting_index) {
        string to_pass = _blocks[0].data.substr(_waiting_index - _blocks[0].index);
        _blocks.pop_front();

        // assume all will be accepted
        size_t written = _output.write(to_pass);
        _waiting_index += written;
        _unassembled_bytes -= written;

        if (written < to_pass.size()) {
            _blocks.push_front({_waiting_index, to_pass.substr(written)});
        }

        cout << written << " bytes writen; " << to_pass.size() << " bytes passed" << endl;
        cout << "_waiting_index: " << _waiting_index << endl;
    }
    cout << "after transition: _blocks.size() = " << _blocks.size() << ", _unassembled_bytes = " << _unassembled_bytes
         << endl;
    for (auto _block : _blocks) {
        std::cout << "    block_index: " << _block.index << ", block_size: " << _block.data.size() << std::endl;
        // cout << "    ";
        // for (size_t i = 0; i < _block.data.size(); i += 1) {
        //     printf("%d ", _block.data.c_str()[i]);
        // }
        // cout << endl;
    }

    finish();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
