#include <string>
#include <libhcs/pcs_t.h>

using namespace std;

/*************************/
/* Http requesting code. */
/*************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <unistd.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

    static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    mem->memory = (char *) realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server-addr> <board-addr>\n", argv[0]);
        return 1;
    }

    const char *addr_server = argv[1];
    const char *addr_board  = argv[2];

    char snpbuf[256];
    srand(time(NULL));

    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;

    chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;             /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

#ifdef LOCAL
    curl_easy_setopt(curl_handle, CURLOPT_PROXYPORT, rand() % 20000 + 40128);
    curl_easy_setopt(curl_handle, CURLOPT_LOCALPORTRANGE, 512);
#endif

    /* specify URL to get */
    snprintf(snpbuf, 256, "http://%s/get_auth_cred", addr_server);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);

    /* get it! */
    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        fprintf(stderr, "Server likely is full\n");
        return 1;
    }

    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * Do something nice with it!
     */

    //fwrite(chunk.memory, 1, chunk.size, stdout);

    /* Set server code */
    pcs_t_auth_server *au = pcs_t_init_auth_server();
    printf("%s\n", chunk.memory);
    pcs_t_import_auth_server(au, chunk.memory);

    /* Now, repeatedly check the public board until the vote is considered
     * over. Once the vote is over, then proceed with retrieving the tally
     * and getting the shared */
    snprintf(snpbuf, 256, "http://%s/get_vote_status", addr_board);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);

    do {
        if (chunk.memory) { free(chunk.memory); chunk.memory = NULL; }
        printf ("Waiting for vote to end...\n");
        sleep(5);
        chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */
        chunk.size = 0;             /* no data at this point */
        res = curl_easy_perform(curl_handle);
    } while (strcmp(chunk.memory, "0") == 0);

    printf("Voting has ceased, get data from server to compute share\n");

    /* Get key before we can combine */
    if (chunk.memory) { free(chunk.memory); chunk.memory = NULL; }
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    snprintf(snpbuf, 256, "http://%s/get_public_key", addr_board);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);
    curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        return 1;
    }

    printf("%s\n", chunk.memory);
    pcs_t_public_key *pk = pcs_t_init_public_key();
    pcs_t_import_public_key(pk, chunk.memory);

    /* Get vote tally and then sum and send a post request back to server for
     * this particular share. */
    if (chunk.memory) { free(chunk.memory); chunk.memory = NULL; }
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    snprintf(snpbuf, 256, "http://%s/get_vote_tally", addr_board);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);
    curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        return 1;
    }

    mpz_t tally;
    mpz_init(tally);
    mpz_set_str(tally, chunk.memory, 62);

    pcs_t_share_decrypt(pk, au, tally, tally);

    char *data = mpz_get_str(NULL, 62, tally);
    string s = "share=" + string(data);

    snprintf(snpbuf, 256, "http://%s/post_share", addr_board);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);
    /* Now specify the POST data */
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, s.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, s.length());
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);

    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        return 1;
    }

    mpz_clear(tally);
    pcs_t_free_public_key(pk);

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();

    return 0;
}
