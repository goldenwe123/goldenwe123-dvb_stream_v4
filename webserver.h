#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>

int callback_http( struct lws *wsi, enum lws_callback_reasons reason, 
				void *user, void *in, size_t len );

#define EXAMPLE_RX_BUFFER_BYTES (10)
struct payload
{
	unsigned char data[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
	size_t len;
} received_payload;

int callback_example( struct lws *wsi, enum lws_callback_reasons reason, 
				void *user, void *in, size_t len );

enum protocols
{
	PROTOCOL_HTTP = 0,
	PROTOCOL_EXAMPLE,
	PROTOCOL_COUNT
};

static struct lws_protocols protocols[] =
{
	/* The first protocol must always be the HTTP handler */
	{
		"http-only",   /* name */
		callback_http, /* callback */
		0,             /* No per session data. */
		0,             /* max frame size / rx buffer */
	},
	{
		"example-protocol",
		callback_example,
		0,
		EXAMPLE_RX_BUFFER_BYTES,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

void web_server_init();

void web_server_run();

void web_server_destroy();

void addwebsocketCallback(void (*on_recevice)(void *in, size_t len ));