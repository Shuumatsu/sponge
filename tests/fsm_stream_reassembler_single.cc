#include "byte_stream.hh"
#include "fsm_stream_reassembler_harness.hh"
#include "stream_reassembler.hh"
#include "util.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ReassemblerTestHarness test{65000};

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }
        printf("111\n");

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"a", 0});

            test.execute(BytesAssembled(1));
            test.execute(BytesAvailable("a"));
            test.execute(NotAtEof{});
        }
        printf("222\n");

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"a", 0}.with_eof(true));

            test.execute(BytesAssembled(1));
            test.execute(BytesAvailable("a"));
            test.execute(AtEof{});
        }
        printf("333\n");

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"", 0}.with_eof(true));

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(AtEof{});
        }
        printf("444\n");

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 0}.with_eof(true));

            test.execute(BytesAssembled(1));
            test.execute(BytesAvailable("b"));
            test.execute(AtEof{});
        }
        printf("555\n");

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"", 0});

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }
        printf("666\n");

        {
            ReassemblerTestHarness test{8};

            test.execute(SubmitSegment{"abcdefgh", 0});

            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable{"abcdefgh"});
            test.execute(NotAtEof{});
        }
        printf("777\n");

        {
            ReassemblerTestHarness test{8};

            test.execute(SubmitSegment{"abcdefgh", 0}.with_eof(true));

            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable{"abcdefgh"});
            test.execute(AtEof{});
        }
        printf("888\n");

        {
            ReassemblerTestHarness test{8};

            test.execute(SubmitSegment{"abc", 0});
            test.execute(BytesAssembled(3));

            test.execute(SubmitSegment{"bcdefgh", 1}.with_eof(true));

            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable{"abcdefgh"});
            test.execute(AtEof{});
        }
        printf("999\n");

        {
            ReassemblerTestHarness test{8};

            test.execute(SubmitSegment{"abc", 0});
            test.execute(BytesAssembled(3));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"ghX", 6}.with_eof(true));
            test.execute(BytesAssembled(3));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"cdefg", 2});
            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable{"abcdefgh"});
            test.execute(NotAtEof{});
        }
        printf("aaa\n");
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
