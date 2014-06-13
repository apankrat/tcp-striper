#include "event_loop.h"
#include <stdio.h>

#include "glue/socket.h"

struct sk_context
{
	int sk;
	event_loop * evl;
};

typedef struct sk_context sk_context;

void on_sk_event(void * context, uint events)
{
	sk_context * skx = (sk_context *)context;

	printf("socket %d, events - %c%c%c\n", 
		skx->sk,
		(events & SK_EV_readable) ? 'R' : '-',
		(events & SK_EV_writable) ? 'W' : '-',
		(events & SK_EV_error)    ? 'X' : '-');

	if (events & SK_EV_writable)
	{
		int err = sk_error(skx->sk);
		if (err)
		{
			printf("Connection failed with %d\n", err);
			skx->evl->del_socket(skx->evl, skx->sk);
		}
		else
		{
			printf("Connected\n");
			skx->evl->mod_socket(skx->evl, skx->sk, SK_EV_readable);
		}
	}

	if (events & SK_EV_error)
	{
		printf("Connection failed with %d\n", sk_error(skx->sk));
		skx->evl->del_socket(skx->evl, skx->sk);
	}
}

int main(int argc, char ** argv)
{
	event_loop * evl;
	sk_context skx;
	sockaddr_in sa;

	//
	evl = new_event_loop_select();

	//
	skx.sk = sk_create(AF_INET, SOCK_STREAM, 0);
	if (skx.sk < 0)
		return 1;

	sk_unblock(skx.sk);
	
	//
	sockaddr_in_init(&sa);
	SOCKADDR_IN_ADDR(&sa) = inet_addr("127.0.0.1");
	SOCKADDR_IN_PORT(&sa) = htons(55555);

	if (sk_connect(skx.sk, (void*)&sa, sizeof sa) < 0 &&
	    sk_conn_fatal( sk_errno(skx.sk) ))
	{
		printf("sk_connect() failed with %d, %d\n", 
			sk_errno(skx.sk), sk_error(skx.sk));
	    	return 2;
	}

	skx.evl = evl;
	evl->add_socket(evl, skx.sk, SK_EV_writable, on_sk_event, &skx);

	while (1)
	{
		evl->monitor(evl, 1000);
		printf("tick\n");
	}

	return 0;
}

