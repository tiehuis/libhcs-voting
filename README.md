This repository demonstrates a simple secure voting system which utilizes
a threshold partially homormorphic cryptosystem.

The library used can be found at https://github.com/Tiehuis/libhcs

Also used is libmicrohttpd for http serving, and libcurl for http requesting.

## To use

    Start a PublicBoard program

    Next, start a SetupServer which connects to the Board

    Next, start a number of DecryptServers and (can be at the same time)
    place a number of votes using the VoterClient program

    When the voting should be finished, then hit a key on the PublicBoard
    process to stop the voting, and wait for all the DecryptServers to send
    their shares of the tally

    When the PublicBoard signals that sufficient shares have been received,
    hit another key to end the vote, displaying the totals

## Problems

- Very messy code, very simple and relies on a lot of assumptions about who
is connecting first

- All these just listen on localhost currently (trivial to alter)

- Difficult to test locally currently, since concurrent instances of
DecryptServer attempt to use the same port and any that are started after the
first will segfault, losing the message that was sent by the server

- And many more, likely
