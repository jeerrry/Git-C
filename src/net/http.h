/*
 * http.h
 *
 * HTTP client for git's smart HTTP protocol.
 * Uses libcurl to handle HTTPS, TLS, and transfer encoding.
 */

#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Holds an HTTP response body accumulated from libcurl callbacks. */
typedef struct {
    char *data;      /* heap-allocated response body (caller must free) */
    size_t size;     /* byte count of data */
} HttpResponse;

/*
 * Fetches the refs list from a git smart HTTP server.
 *
 * Sends: GET <url>/info/refs?service=git-upload-pack
 * The response contains pkt-line formatted ref advertisements.
 *
 * @param url   Repository URL (e.g. "https://github.com/user/repo")
 * @param resp  Output: populated with the response body on success.
 * @return      0 on success, 1 on failure.
 */
int http_get_refs(const char *url, HttpResponse *resp);

/*
 * Sends a git-upload-pack request to fetch a packfile.
 *
 * Sends: POST <url>/git-upload-pack with the given body.
 * The response contains a packfile (after pkt-line framing).
 *
 * @param url       Repository URL.
 * @param body      Request body (pkt-line formatted "want" lines).
 * @param body_len  Byte count of body.
 * @param resp      Output: populated with the response body on success.
 * @return          0 on success, 1 on failure.
 */
int http_post_pack(const char *url, const char *body, size_t body_len, HttpResponse *resp);

/*
 * Frees the data inside an HttpResponse.
 * Safe to call on a zeroed or already-freed response.
 */
void http_response_free(HttpResponse *resp);

#endif /* HTTP_H */
