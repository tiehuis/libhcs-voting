#include <gmp.h>
#include <libhcs/pcs_t.h>

using namespace std;

class Voter {

public:
    pcs_t_public_key *pk;
    hcs_random *hr;
    mpz_t rop;

    Voter() {
        mpz_init(rop);
        hr = hcs_init_rand();
        pk = pcs_t_init_public_key();
    }

    ~Voter() {
        mpz_clear(rop);
        hcs_free_rand(hr);
        pcs_t_free_public_key(pk);
    }

    void make_vote(mpz_t rop, mpz_t op) {
        pcs_t_encrypt(pk, hr, rop, op);
    }

    void import_key(char *json) {
        pcs_t_import_public_key(pk, json);
    }
};

/*************************/
/* Http requesting code. */
/*************************/

#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>

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
    if (argc < 3) {
        printf("VoterClient <board-addr> <vote>\n");
        return 1;
    }

    const char *addr = argv[1];
    char snpbuf[256];
    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;

    chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;             /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    snprintf(snpbuf, 256, "http://%s/get_public_key", addr);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* get it! */
    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    } else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        fwrite(chunk.memory, 1, chunk.size, stdout);
        printf("\n");
    }

    curl_easy_cleanup(curl_handle);

    Voter v;
    mpz_t r;

    mpz_init(r);
    mpz_set_str(r, argv[2], 10);

    pcs_t_import_public_key(v.pk, chunk.memory);
    pcs_t_encrypt(v.pk, v.hr, r, r);

    free(chunk.memory);

    /* Now send a post request back to sever with our vote */
    string req = "vote=";
    req += string(mpz_get_str(NULL, 62, r));
    cout << req;

    curl_handle = curl_easy_init();
    snprintf(snpbuf, 256, "http://%s/post_vote", addr);
    curl_easy_setopt(curl_handle, CURLOPT_URL, snpbuf);
    /* Now specify the POST data */
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, req.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, req.length());
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl_handle);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();

    return 0;
}
