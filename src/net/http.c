/*
 * http.c
 *
 * HTTP client using libcurl for git's smart HTTP protocol.
 * Two operations: GET refs (discover what the server has) and
 * POST upload-pack (request a packfile of objects).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "../constants.h"
#include "http.h"

/*
 * libcurl callback — called repeatedly as data arrives from the server.
 *
 * libcurl doesn't give us the whole response at once. It calls this
 * function multiple times with chunks of data (e.g., 16KB at a time).
 * We grow our buffer with realloc and append each chunk.
 *
 * Parameters (set by libcurl, not us):
 *   chunk     — pointer to the incoming data
 *   elem_size — always 1 (size of each element)
 *   count     — number of elements (= number of bytes)
 *   userdata  — our HttpResponse pointer (passed via CURLOPT_WRITEDATA)
 *
 * Must return the number of bytes handled. Returning less than
 * count signals an error and aborts the transfer.
 */
static size_t write_callback(void *chunk, size_t elem_size, size_t count, void *userdata) {
    size_t chunk_size = elem_size * count;
    HttpResponse *resp = (HttpResponse *)userdata;

    /* Grow the buffer to fit the new chunk.
     * +1 for a null terminator we add for convenience (so the data
     * can be used as a C string for text responses like refs). */
    char *new_data = realloc(resp->data, resp->size + chunk_size + 1);
    if (new_data == NULL) {
        GIT_ERR("realloc failed in HTTP callback\n");
        return 0;
    }

    resp->data = new_data;
    memcpy(resp->data + resp->size, chunk, chunk_size);
    resp->size += chunk_size;
    resp->data[resp->size] = '\0';

    return chunk_size;
}

/*
 * Shared setup for both GET and POST requests.
 * Returns a configured CURL handle, or NULL on failure.
 */
static CURL *setup_curl(const char *url, HttpResponse *resp) {
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        GIT_ERR("curl_easy_init failed\n");
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    /* Follow HTTP redirects (GitHub sometimes redirects) */
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    /* User-Agent header — some servers reject requests without one */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "git/codecrafters");

    return curl;
}

/*
 * Runs a curl request and checks for errors.
 * Cleans up the curl handle before returning.
 */
static int perform_and_cleanup(CURL *curl) {
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        GIT_ERR("HTTP request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return 1;
    }

    long http_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        GIT_ERR("HTTP error: %ld\n", http_code);
        return 1;
    }

    return 0;
}

int http_get_refs(const char *url, HttpResponse *resp) {
    resp->data = NULL;
    resp->size = 0;

    /* Build the refs discovery URL:
     * <repo_url>.git/info/refs?service=git-upload-pack */
    char full_url[GIT_PATH_MAX];
    snprintf(full_url, sizeof(full_url), "%s.git/info/refs?service=git-upload-pack", url);

    CURL *curl = setup_curl(full_url, resp);
    if (curl == NULL) return 1;

    return perform_and_cleanup(curl);
}

int http_post_pack(const char *url, const char *body, size_t body_len, HttpResponse *resp) {
    resp->data = NULL;
    resp->size = 0;

    /* Build the upload-pack URL: <repo_url>.git/git-upload-pack */
    char full_url[GIT_PATH_MAX];
    snprintf(full_url, sizeof(full_url), "%s.git/git-upload-pack", url);

    CURL *curl = setup_curl(full_url, resp);
    if (curl == NULL) return 1;

    /* Set POST method with the request body */
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_len);

    /* Git protocol requires this specific Content-Type */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-git-upload-pack-request");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    int result = perform_and_cleanup(curl);
    curl_slist_free_all(headers);
    return result;
}

void http_response_free(HttpResponse *resp) {
    free(resp->data);
    resp->data = NULL;
    resp->size = 0;
}
