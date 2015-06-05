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

An example of the commands that would be run:

    term1$ ./PublicBoard 40000
    term2$ ./SetupServer 40001 127.0.0.1:40000
    term3$ ./VotingClient 127.0.0.1:40001 1
    term3$ ./VotingClient 127.0.0.1:40001 0
    term3$ ./VotingClient 127.0.0.1:40001 0
    term1$ [enter]
    term4$ ./DecryptServer 127.0.0.1:40000 127.0.0.1:40001
    term1$ [enter]
    term1$ #Expected output of the combined shares would be 1

## Problems

- Very messy code, very simple and relies on a lot of assumptions about who
is connecting first

- ~~All these just listen on localhost currently (trivial to alter)~~

- Difficult to test locally currently, since concurrent instances of
DecryptServer attempt to use the same port and any that are started after the
first will segfault, losing the message that was sent by the server

- And many more, likely
