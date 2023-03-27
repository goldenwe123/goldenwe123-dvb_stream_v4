
#include "webserver.h"

struct lws_context *context;

static void (*on_websocket_recevice)(void *in, size_t len ) = NULL; 

int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{	
	//printf("callback_http %d\n",reason);
	switch( reason )
	{	
		
		case LWS_CALLBACK_HTTP:
			//printf("callback_http %d\n",reason);
			lws_serve_http_file( wsi, "example.html", "text/html", NULL, 0 );
			//lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
			break;
		default:
			break;
	}
	
	return 0;
}

void addwebsocketCallback(void (*on_recevice)(void *in, size_t len ))
{
	on_websocket_recevice = on_recevice;
	
}	

int callback_example( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{	
	char* cc;
	//printf("callback_example %d\n",reason);
	switch( reason )
	{
		case LWS_CALLBACK_ESTABLISHED:
			//printf("LWS_CALLBACK_ESTABLISHED\n");
			//lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
		case LWS_CALLBACK_RECEIVE:
			//printf("LWS_CALLBACK_RECEIVE\n");
			//printf("Len: %d\n",len);
			
			if(on_websocket_recevice != NULL)
				on_websocket_recevice(in, len);
			
			//lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			//printf("LWS_CALLBACK_SERVER_WRITEABLE\n");
			lws_write( wsi, &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], received_payload.len, LWS_WRITE_TEXT );
			break;

		default:
			break;
	}
	return 0;
}

void web_server_init()
{
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = 8000;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
	
	context = lws_create_context( &info );
	
}

void web_server_run()
{
	lws_service( context, /* timeout_ms = */ 1000000 );
}

void web_server_destroy()
{
	lws_context_destroy( context );
}
	
	