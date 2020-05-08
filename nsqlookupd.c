#include "nsq.h"

int nsq_lookupd_connect_producer(nsqLookupdEndpoint *lookupd, const int count, const char *topic,
    httpClient *httpc, void *arg, int mode)
{
    nsqLookupdEndpoint *nsqlookupd_endpoint;
    httpRequest *req;
    int i = 0, idx;
    char buf[256];

    idx = rand() % count;

    _DEBUG("%s: lookupd %p (chose %d), topic %s, httpClient %p", __FUNCTION__, lookupd, idx, topic, httpc);

    LL_FOREACH(lookupd, nsqlookupd_endpoint) {
        if (i == idx) {
            sprintf(buf, "http://%s:%d/lookup?topic=%s", nsqlookupd_endpoint->address,
                nsqlookupd_endpoint->port, topic);
            req = new_http_request(buf, nsq_lookupd_request_cb, arg);
            http_client_get(httpc, req);
            break;
        }
    }
    
    return idx;
}

void nsq_lookupd_request_cb(httpRequest *req, httpResponse *resp, void *arg, int mode)
{
    nsq_json_t *jsobj, *producers;
    nsq_json_tokener_t *jstok;

    _DEBUG("%s: status_code %d, body %.*s", __FUNCTION__, resp->status_code,
        (int)BUFFER_HAS_DATA(resp->data), resp->data->data);

    if (resp->status_code != 200) {
        free_http_response(resp);
        free_http_request(req);
        return;
    }

    jstok = nsq_json_tokener_new();
    jsobj = nsq_json_loadb(resp->data->data, (nsq_json_size_t)BUFFER_HAS_DATA(resp->data), 0, jstok);
    if (!jsobj) {
        _DEBUG("%s: error parsing JSON", __FUNCTION__);
        nsq_json_tokener_free(jstok);
        return;
    }

    nsq_json_object_get(jsobj, "producers", &producers);
    if (!producers) {
        _DEBUG("%s: error getting 'producers' key", __FUNCTION__);
        nsq_json_decref(jsobj);
        nsq_json_tokener_free(jstok);
        return;
    }

    _DEBUG("%s: num producers %ld", __FUNCTION__, (long)nsq_json_array_length(producers));
    if (mode == NSQ_LOOKUPD_MODE_READ) {
        nsq_reader_loop_producers(producers, (nsqReader *)arg);
    } else if (mode == NSQ_LOOKUPD_MODE_WRITE) {
        nsq_reader_loop_producers(producers, (nsqWriter *)arg);
    }

    nsq_json_decref(jsobj);
    nsq_json_tokener_free(jstok);

    free_http_response(resp);
    free_http_request(req);
}

nsqLookupdEndpoint *new_nsqlookupd_endpoint(const char *address, int port)
{
    nsqLookupdEndpoint *nsqlookupd_endpoint;

    nsqlookupd_endpoint = (nsqLookupdEndpoint *)malloc(sizeof(nsqLookupdEndpoint));
    nsqlookupd_endpoint->address = strdup(address);
    nsqlookupd_endpoint->port = port;
    nsqlookupd_endpoint->next = NULL;

    return nsqlookupd_endpoint;
}

void free_nsqlookupd_endpoint(nsqLookupdEndpoint *nsqlookupd_endpoint)
{
    if (nsqlookupd_endpoint) {
        free(nsqlookupd_endpoint->address);
        free(nsqlookupd_endpoint);
    }
}
