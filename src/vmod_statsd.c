#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

// Socket related libraries
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#define BUF_SIZE 500
#define INCREMENT ":1|c"

#ifdef DEBUG                    // To print diagnostics to the error log
#define _DEBUG 1                // enable through gcc -DDEBUG
#else
#define _DEBUG 0
#endif

typedef struct statsdConfig {
	char *host;     // statsd host
	char *port;     // statsd port - as STRING
	int socket;   // open socket to the daemon
} config_t;


int
_send_to_statsd( struct vmod_priv *priv, const char *stat ) {
    config_t *cfg = priv->priv;

    _DEBUG && fprintf( stderr, "send: %s:%s %s\n", cfg->host, cfg->port, stat );

    // ******************************
    // Sanity checks
    // ******************************

    int len = strlen( stat ) + 1;  // +1 for the null byte
    if( len >= BUF_SIZE ) {
        _DEBUG && fprintf( stderr, "Message length %d > max length %d - ignoring\n",
            len, BUF_SIZE );
        return -1;
    }

    // ******************************
    // Send the packet
    // ******************************

    if( write( cfg->socket, stat, len ) != len ) {
        _DEBUG && fprintf( stderr, "Partial/failed write for %s\n", stat );
        return -1;
    }

}

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {
	return (0);
}


/** The following may ONLY be called from VCL_init **/
void
vmod_server( struct sess *sp, struct vmod_priv *priv, const char *host, const char *port ) {

    // ******************************
    // Configuration
    // ******************************

    config_t *cfg;
    cfg         = malloc(sizeof(config_t));
    cfg->host   = strdup( host );
    cfg->port   = strdup( port );


    // ******************************
    // Connect to the remote socket
    // ******************************

    struct addrinfo *statsd;
    statsd = malloc(sizeof(struct addrinfo));

    struct addrinfo *hints;
    hints = malloc(sizeof(struct addrinfo));


    // what type of socket is the statsd endpoint?
    hints->ai_family   = AF_INET;
    hints->ai_socktype = SOCK_DGRAM;
    hints->ai_protocol = IPPROTO_UDP;
    hints->ai_flags    = 0;

    // using getaddrinfo lets us use a hostname, rather than an
    // ip address.
    int err;
    if( err = getaddrinfo( cfg->host, cfg->port, hints, &statsd ) != 0 ) {
        _DEBUG && fprintf( stderr, "getaddrinfo on %s:%s failed: %s\n",
            cfg->host, cfg->port, gai_strerror(err) );
        return;
    }

    // getaddrinfo() may return more than one address structure
    // but since this is UDP, we can't verify the connection
    // anyway, so we will just use the first one
    cfg->socket = socket( statsd->ai_family, statsd->ai_socktype,
                       statsd->ai_protocol );

    if( cfg->socket == -1 ) {
        _DEBUG && fprintf( stderr, "socket creation failed\n" );
        close( cfg->socket );
        return;
    }

    // connection failed.. for some reason...
    if( connect( cfg->socket, statsd->ai_addr, statsd->ai_addrlen ) == -1 ) {
        _DEBUG && fprintf( stderr, "socket connection failed\n" );
        close( cfg->socket );
        return;
    }

    // now that we have an outgoing socket, we don't need this
    // anymore.
    freeaddrinfo( statsd );
    freeaddrinfo( hints );

    // ******************************
    // Store the config
    // ******************************

    priv->priv = cfg;

    _DEBUG && printf( "statsd server: %s:%s (fd: %d)\n",
                cfg->host, cfg->port, cfg->socket );
}

void
vmod_incr( struct sess *sp, struct vmod_priv *priv, const char *key ) {
    _DEBUG && printf( "incr: %s\n", key );

    // Get the buffer ready. +5 for the counter + null byte
    char stat[ strlen(key) + 5 ];

    // -4 to make sure there's room for the meta chars, but leave room
    // now for the null byte
    // XXX is using strNcpy overkill when the buffer is of known size?
    // Looks like: gorets:1|c
    strncpy( stat, key, sizeof(stat) - 4 );
    strncat( stat, ":1|c", 4 );

    // Incremenet is straight forward - just add the count + type
    _send_to_statsd( priv, stat );
}

void
vmod_timing( struct sess *sp, struct vmod_priv *priv, const char *key, int val ) {
    _DEBUG && printf( "timing: %s = %d\n", key, val );

    // Get the buffer ready. +4 for the metachars + null byte
    char stat[ strlen(key) + sizeof( val ) + 4 ];

    // looks like glork:320|ms
    snprintf( stat, sizeof(stat) - 1, "%s:%d|ms", key, val );

    _send_to_statsd( priv, stat );
}

void
vmod_counter( struct sess *sp, struct vmod_priv *priv, const char *key, int val ) {
    _DEBUG && printf( "counter: %s = %d\n", key, val );

    // Get the buffer ready. +3 for the metachars + null byte
    char stat[ strlen(key) + sizeof( val ) + 3 ];

    // looks like: gorets:42|c
    snprintf( stat, sizeof(stat) - 1, "%s:%d|c", key, val );

    _send_to_statsd( priv, stat );
}

void
vmod_gauge( struct sess *sp, struct vmod_priv *priv, const char *key, int val ) {
    _DEBUG && printf( "gauge: %s = %d\n", key, val );

    // Get the buffer ready. +3 for the metachars + null byte
    char stat[ strlen(key) + sizeof( val ) + 3 ];

    // looks like: gaugor:333|g
    snprintf( stat, sizeof(stat) - 1, "%s:%d|g", key, val );

    _send_to_statsd( priv, stat );
}


// const char *
// vmod_hello(struct sess *sp, const char *name)
// {
// 	char *p;
// 	unsigned u, v;
//
// 	u = WS_Reserve(sp->wrk->ws, 0); /* Reserve some work space */
// 	p = sp->wrk->ws->f;		/* Front of workspace area */
// 	v = snprintf(p, u, "Hello, %s", name);
// 	v++;
// 	if (v > u) {
// 		/* No space, reset and leave */
// 		WS_Release(sp->wrk->ws, 0);
// 		return (NULL);
// 	}
// 	/* Update work space with what we've used */
// 	WS_Release(sp->wrk->ws, v);
// 	return (p);
// }
