#include <cstring>
#include <iostream>
#include <string>
#include <gmp.h>
#include <libhcs.h>

using namespace std;

class SetupServer {

    private:
        pcs_t_public_key *pk;
        pcs_t_private_key *vk;
        pcs_t_polynomial *px;
        hcs_random *hr;
        unsigned long s;
        unsigned long l;
        unsigned long ars;

    public:
        SetupServer(unsigned long s_, unsigned long l_) : s(s_), l(l_), ars(0) {
            pk = pcs_t_init_public_key();
            vk = pcs_t_init_private_key();
            hr = hcs_init_random();

            /* Need to improve safe prime generation performance before using
             * larger primes here. */
            pcs_t_generate_key_pair(pk, vk, hr, 256, s, l);
            px = pcs_t_init_polynomial(vk, hr);
        }

        ~SetupServer() {
            pcs_t_free_public_key(pk);
            pcs_t_free_private_key(vk);
            pcs_t_free_polynomial(px);
            hcs_free_random(hr);
        }

        unsigned long auth_req_seen() {
            return ars;
        }

        unsigned long server_count() {
            return l;
        }

        char* get_public_key() {
            return pcs_t_export_public_key(pk);
        }

        void inc_server_count() {
            ars++;
        }

        int servers_left() {
            return ars < l;
        }

        string get_vote_info() {
            string json;

            json  = "{\"public_key\":";
            json += string(pcs_t_export_public_key(pk));
            json += ",\"verification_values\":"; // Always empty currently
            json += string(pcs_t_export_verify_values(vk));
            json += "}";
            return json;
        }

        string get_auth_info() {
            string json;
            mpz_t rop;

            mpz_init(rop);
            pcs_t_compute_polynomial(vk, px, rop, ars);
            json  = "{\"si\":\"";
            json += string(mpz_get_str(NULL, 62, rop));
            json += "\",\"i\":";
            json += to_string(ars);
            json += "}";
            mpz_clear(rop);
            return json;
        }

};

/* Send the initial key data to the public board */

#include <curl/curl.h>

void curl_send(SetupServer *s, char *addr)
{
    char buffer[256];
    CURL *curl;
    CURLcode res;

    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
           just as well be a https:// URL if that is what should receive the
           data. */

        char *data = s->get_public_key();

        string s = "public_key=" + string(data);

        snprintf(buffer, 256, "http://%s/post_key", addr);
        curl_easy_setopt(curl, CURLOPT_URL, buffer);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, s.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, s.length());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        free(data);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

/********************/
/* Http server code */
/********************/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <microhttpd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

static SetupServer *s;

int sate_request(void *cls, struct MHD_Connection *connection,
        const char *url, const char *method, const char *version,
        const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    int ret;

    const MHD_ConnectionInfo *conninfo = MHD_get_connection_info(connection,
            MHD_CONNECTION_INFO_CLIENT_ADDRESS);

    printf("Port: %d\n", ntohs(((sockaddr_in*)(&conninfo->client_addr))->sin_port));

    /* This server only allows get requests */
    if (strcmp(method, "GET") != 0)
        return MHD_NO;

    /* Only return data for page /get_auth_cred */
    if (strcmp(url, "/get_auth_cred") != 0 || !s->servers_left()) {
        const char *rets = "No servers remaining";
        struct MHD_Response *response = MHD_create_response_from_buffer(
                strlen(rets), (void*)rets, MHD_RESPMEM_MUST_COPY);
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
    }
    else {
        const char *str = s->get_auth_info().c_str();
        struct MHD_Response *response = MHD_create_response_from_buffer(
                strlen(str), (void*)str, MHD_RESPMEM_MUST_COPY);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        s->inc_server_count();
    }

    return ret;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <board-addr>\n", argv[0]);
        return 1;
    }

    s = new SetupServer(1, 3);
    const int port_arg = atoi(argv[1]);
    curl_send(s, argv[2]);

    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port_arg,
            NULL, NULL, &sate_request, NULL, MHD_OPTION_END);

    if (!daemon)
        return 1;

    getchar();

    MHD_stop_daemon(daemon);
    delete s;
    return 0;
}
