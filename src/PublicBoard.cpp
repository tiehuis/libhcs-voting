#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <libhcs/pcs_t.h>

using namespace std;

class PublicBoard {

public:
    vector<string> votes;
    vector<string> shares;
    int frozen;
    string result;
    string tally;
    string public_key;
    pcs_t_public_key *pk;
    hcs_rand *hr;
    mpz_t t1, t2, tally_mpz, share_mpz;
    int servers_seen;

    PublicBoard() : frozen(0), servers_seen(0) {
        pk = pcs_t_init_public_key();
        hr = hcs_init_rand();
        mpz_inits(t1, t2, tally_mpz, share_mpz, NULL);
    }

    ~PublicBoard() {
        pcs_t_free_public_key(pk);
        hcs_free_rand(hr);
        mpz_clears(t1, t2, tally_mpz, share_mpz, NULL);
    }

    void display(void) {
        cout << "\nVotes\n";
        for (auto &i : votes)
            cout << i << "\n";

        cout << "\nTally: " << string(get_vote_tally()) << "\n";

        cout << "\nShares\n";
        for (auto &i : shares)
            cout << i << "\n";

        cout << "\nCombined: " << string(get_share_str()) << "\n";
    }

    void sum_votes(void) {
        mpz_set_ui(tally_mpz, 0);
        pcs_t_encrypt(pk, hr, tally_mpz, tally_mpz);
        for (auto &i : votes) {
            mpz_set_str(t1, i.c_str(), 62);
            pcs_t_ee_add(pk, tally_mpz, tally_mpz, t1);
        }
    }

    void sum_shares(void) {
        mpz_t share_loc[pk->l];
        for (int i = 0; i < pk->l; ++i) mpz_init(share_loc[i]);
        mpz_set_ui(share_mpz, 0);

        int j = 0;
        for (auto &i : shares) mpz_set_str(share_loc[j++], i.c_str(), 62);

        pcs_t_share_combine(pk, share_mpz, share_loc);
        for (int i = 0; i < pk->l; ++i) mpz_clear(share_loc[i]);
    }

    void add_vote(string vote) {
        votes.push_back(vote);
    }

    void add_share(string share) {
        shares.push_back(share);
    }

    char* get_vote_tally(void) {
        return mpz_get_str(NULL, 62, tally_mpz);
    }

    char* get_share_str(void) {
        return mpz_get_str(NULL, 62, share_mpz);
    }

    void set_result(string result_) {
        result = result_;
    }

    void end_vote() {
        frozen = 1;
    }

    int vote_ended() {
        return frozen;
    }
};

#include <microhttpd.h>

static PublicBoard *p;

const char *errormsg = "Failed to satisfy request";

enum { GET, POST };

struct con_info_s {
    struct MHD_PostProcessor *pp;
    char *answer;
    int type;
};

int send_page (struct MHD_Connection *connection, const char *page)
{
  int ret;
  struct MHD_Response *response;


  response =
    MHD_create_response_from_buffer (strlen (page), (void *) page,
				     MHD_RESPMEM_MUST_COPY);
  if (!response)
    return MHD_NO;

  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);

  return ret;
}

int iterate_post(void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
        const char *filename, const char *content_type,
        const char *transfer_encoding, const char *data, uint64_t off,
        size_t size)
{
    struct con_info_s *con_info = (struct con_info_s*)coninfo_cls;

    /* Discard trailing empty post */
    if (strcmp(key, "public_key") == 0 && size != 0) {
        p->public_key.assign(data, size);
        con_info->answer = strdup("Public key received!\n");
        pcs_t_import_public_key(p->pk, p->public_key.c_str());
    }
    else if (strcmp(key, "vote") == 0 && size != 0) {
        p->add_vote(string(data));
        con_info->answer = strdup("Vote received!\n");
    }
    else if (strcmp(key, "share") == 0  && size != 0) {
        printf("%s\n", data);
        p->add_share(string(data));
        con_info->answer = strdup("Share received!\n");
        printf("%d shares received\n", ++p->servers_seen);
        if (p->servers_seen >= p->pk->w)
            printf("No more servers required, can tally votes\n");
    }
    else {
        return MHD_NO;
    }
    return MHD_YES;
}

void request_completed(void *cls, struct MHD_Connection *connection,
        void **con_cls, enum MHD_RequestTerminationCode toe)
{
    struct con_info_s *con_info = (struct con_info_s*)*con_cls;

    if (con_info == NULL)
        return;

    if (con_info->type == POST) {
        MHD_destroy_post_processor(con_info->pp);
        if (con_info->answer)
            free(con_info->answer);
    }

    free(con_info);
    *con_cls = NULL;
}

int sate_request(void *cls, struct MHD_Connection *connection,
        const char *url, const char *method, const char *version,
        const char *upload_data, size_t *upload_data_size, void **con_cls)
{

    if (*con_cls == NULL) {
        struct con_info_s *con_info = (struct con_info_s*)malloc(sizeof(struct con_info_s));
        if (strcmp(method, "POST") == 0) {
            con_info->pp = MHD_create_post_processor(connection, 512,
                    iterate_post, con_info);
            con_info->type = POST;
        }
        else {
            con_info->type = GET;
        }

        *con_cls = (void*)con_info;
        return MHD_YES;
    }

    const char *buffer = NULL;

    /* Post request */
    if (strcmp(method, "POST") == 0) {

        struct con_info_s *con_info = (struct con_info_s*)*con_cls;

        if (*upload_data_size != 0) {
            MHD_post_process(con_info->pp, upload_data, *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        }
        else if (con_info->answer != NULL)
            return send_page(connection, con_info->answer);
    }

    /* Get request */
    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/get_public_key") == 0) {
            buffer = p->public_key.c_str();
        }
        else if (strcmp(url, "/get_vote_status") == 0) {
            buffer = to_string(p->vote_ended()).c_str();
        }
        else if (strcmp(url, "/get_vote_tally") == 0) {
            buffer = p->get_vote_tally();
        }
    }

    if (buffer)
        return send_page(connection, buffer);

    return send_page(connection, errormsg);
}

int main(void)
{
    p = new PublicBoard();
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 40001,
            NULL, NULL, &sate_request, NULL, MHD_OPTION_NOTIFY_COMPLETED,
            &request_completed, NULL, MHD_OPTION_END);

    if (!daemon) return 1;

    getchar();
    p->end_vote();
    p->sum_votes();
    p->display();

    /* Wait for all shares from each server or until a specified timeout */
    printf("Waiting for all server shares...\n");
    getchar();
    p->sum_shares();
    p->display();

    MHD_stop_daemon(daemon);
    delete p;
    return 0;
}
