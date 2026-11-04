/* SPDX-License-Identifier: Apache-2.0 */
#include "book_reader_input.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    EpdRect body={0,0,600,900};
    book_reader_tap_t tap={0};
    assert(book_reader_tap_feed(&tap,body,320,450,false,false,100)==1);
    assert(book_reader_tap_feed(&tap,body,320,450,false,false,200)==1);
    assert(!tap.pending && !book_reader_tap_tick(&tap,600,false));
    assert(book_reader_tap_feed(&tap,body,280,450,false,false,700)==-1);
    assert(book_reader_tap_feed(&tap,body,320,450,false,true,800)==-1);
    assert(book_reader_tap_feed(&tap,body,320,450,true,false,900)==1);
    assert(!book_reader_tap_feed(&tap,body,600,450,false,false,1000));
    assert(!book_reader_tap_feed_any(&tap,body,320,450,false,false,1100));
    assert(book_reader_tap_feed_any(&tap,body,320,450,false,false,1200)==BOOK_READER_TAP_DOUBLE);
    assert(!book_reader_tap_tick(&tap,1600,false));
    assert(!book_reader_tap_feed_any(&tap,body,320,450,false,false,2000));
    assert(!book_reader_tap_tick(&tap,2340,false));
    assert(book_reader_tap_tick(&tap,2341,false)==1);
    assert(!book_reader_tap_feed_any(&tap,body,320,450,false,false,3000));
    assert(book_reader_tap_feed(&tap,body,320,450,false,false,3100)==1);
    assert(!book_reader_tap_tick(&tap,3500,false));
    puts("PASS: independent immediate text taps, direction modes, bounds, illustration double/single tap and pending cancellation");
}
