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

//#define DEBUG 1

#ifdef DEBUG                    // To print diagnostics to the error log
#define _DEBUG 1                // enable through gcc -DDEBUG
#else
#define _DEBUG 0
#endif

typedef struct statsdConfig {
	char *host;     // statsd host
	char *port;     // statsd port - as STRING
	char *prefix;   // prefix any key with this
	char *suffix;   // suffix any key with this
	int socket;     // open socket to the daemon
} config_t;


// ******************************
// Utility functions
// ******************************

// Unfortunately std.fileread() will append a newline, even if there is none in
// the file that was being read. So if you use that (as we suggest in the docs)
// to set prefix/suffix, you'll be in for a nasty surprise.
char *
_strip_newline( char *line ) {
    char *pos;

    if( (pos = strchr( line, '\n' )) != NULL ) {
        *pos = '\0';
    }

    if( (pos = strchr( line, '\r' )) != NULL ) {
        *pos = '\0';
    }

    //_DEBUG && fprintf( stderr, "stripping new lines. New string: %s\n", line );


    return line;
}

// ******************************
// Configuration
// ******************************

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {

    // ******************************
    // Configuration defaults
    // ******************************

    config_t *cfg;
    cfg         = malloc(sizeof(config_t));
    cfg->host   = "localhost";
    cfg->port   = "8125";
    cfg->prefix = "";
    cfg->suffix = "";

    _DEBUG && fprintf( stderr, "init: configuration initialized\n" );

    // ******************************
    // Store the config
    // ******************************

    priv->priv = cfg;

	return (0);
}

/** The following may ONLY be called from VCL_init **/
void
vmod_prefix( struct sess *sp, struct vmod_priv *priv, const char *prefix ) {
    config_t *cfg = priv->priv;
    cfg->prefix = _strip_newline( strdup( prefix ) );
}

/** The following may ONLY be called from VCL_init **/
void
vmod_suffix( struct sess *sp, struct vmod_priv *priv, const char *suffix ) {

    config_t *cfg = priv->priv;
    cfg->suffix = _strip_newline( strdup( suffix ) );
}

/** The following may ONLY be called from VCL_init **/
void
vmod_server( struct sess *sp, struct vmod_priv *priv, const char *host, const char *port ) {

    // ******************************
    // Configuration
    // ******************************

    config_t *cfg = priv->priv;
    cfg->host   = strdup( host );
    cfg->port   = strdup( port );
}


// ******************************
// Connect to the remote socket
// ******************************

int
_connect_to_statsd( struct vmod_priv *priv ) {
    config_t *cfg = priv->priv;

    // Grab 2 structs for the connection
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
        return -1;
    }


    // ******************************
    // Store the open connection
    // ******************************

    // getaddrinfo() may return more than one address structure
    // but since this is UDP, we can't verify the connection
    // anyway, so we will just use the first one
    cfg->socket = socket( statsd->ai_family, statsd->ai_socktype,
                       statsd->ai_protocol );

    if( cfg->socket == -1 ) {
        _DEBUG && fprintf( stderr, "socket creation failed\n" );
        close( cfg->socket );
        return -1;
    }

    // connection failed.. for some reason...
    if( connect( cfg->socket, statsd->ai_addr, statsd->ai_addrlen ) == -1 ) {
        _DEBUG && fprintf( stderr, "socket connection failed\n" );
        close( cfg->socket );
        return -1;
    }

    // now that we have an outgoing socket, we don't need this
    // anymore.
    freeaddrinfo( statsd );
    freeaddrinfo( hints );

    _DEBUG && printf( "statsd server: %s:%s (fd: %d)\n",
                cfg->host, cfg->port, cfg->socket );

    return cfg->socket;
}

int
_send_to_statsd( struct vmod_priv *priv, const char *key, const char *val ) {
    config_t *cfg = priv->priv;
   
    // If you are using some empty key, bail - this can happen if you use
    // say: statsd.incr( req.http.x-does-not-exist ). Rather than getting
    // and empty string, we get a null pointer.
    if( key == NULL || val == NULL ) {
        _DEBUG && fprintf( stderr, "Key or value is NULL pointer - ignoring\n" );
        return -1;
    }

    // Enough room for the key/val plus prefix/suffix plus newline plus a null byte.
    char stat[ strlen(key) + strlen(val) +
               strlen(cfg->prefix) + strlen(cfg->suffix) + 1 ];

    strncpy( stat, cfg->prefix, strlen(cfg->prefix) + 1 );
    strncat( stat, key,         strlen(key)         + 1 );
    strncat( stat, cfg->suffix, strlen(cfg->suffix) + 1 );
    strncat( stat, val,         strlen(val)         + 1 );

    // Newer versions of statsd allow multiple metrics in a single packet, delimited
    // by newlines. That unfortunately means that if we end our message with a new
    // line, statsd will interpret this as an empty second metric and log a 'bad line'.
    // This is true in at least version 0.5.0 and to avoid that, we don't send the 
    // newline. Makes debugging using nc -klu 8125 a bit more tricky, but works with
    // modern statsds.
    //strncat( stat, "\n",        1                       );

    _DEBUG && fprintf( stderr, "send: %s:%s %s\n", cfg->host, cfg->port, stat );

    // ******************************
    // Sanity checks
    // ******************************

    int len = strlen( stat );

    // +1 for the null byte
    if( len + 1 >= BUF_SIZE ) {
        _DEBUG && fprintf( stderr, "Message length %d > max length %d - ignoring\n",
            len, BUF_SIZE );
        return -1;
    }

    // ******************************
    // Send the packet
    // ******************************

    // we may not have connected yet - in that case, do it now
    int sock = cfg->socket > 0 ? cfg->socket : _connect_to_statsd( priv );

    // If we didn't get a socket, don't bother trying to send
    if( sock == -1 ) {
        _DEBUG && fprintf( stderr, "Could not get socket for %s\n", stat );
        return -1;
    }

    // Send the stat
    int sent = write( sock, stat, len );

    _DEBUG && fprintf( stderr, "Sent %d of %d bytes to FD %d\n", sent, len, sock );

    // Should we unset the socket if this happens?
    if( sent != len ) {
        _DEBUG && fprintf( stderr, "Partial/failed write for %s\n", stat );
        return -1;
    }

    return 0;
}


void
vmod_incr( struct sess *sp, struct vmod_priv *priv, const char *key ) {
    _DEBUG && printf( "incr: %s\n", key );

    // Incremenet is straight forward - just add the count + type
    _send_to_statsd( priv, key, ":1|c" );
}

void
vmod_timing( struct sess *sp, struct vmod_priv *priv, const char *key, int num ) {
    _DEBUG && printf( "timing: %s = %d\n", key, num );

    // Get the buffer ready. 10 for the maximum lenghth of an int and +5 for metadata
    char val[ 15 ];

    // looks like glork:320|ms
    snprintf( val, sizeof(val), ":%d|ms", num );

    _send_to_statsd( priv, key, val );
}

void
vmod_counter( struct sess *sp, struct vmod_priv *priv, const char *key, int num ) {
    _DEBUG && printf( "counter: %s = %d\n", key, num );

    // Get the buffer ready. 10 for the maximum lenghth of an int and +5 for metadata
    char val[ 15 ];

    // looks like: gorets:42|c
    snprintf( val, sizeof(val), ":%d|c", num );

    _send_to_statsd( priv, key, val );
}

void
vmod_gauge( struct sess *sp, struct vmod_priv *priv, const char *key, int num ) {
    _DEBUG && printf( "gauge: %s = %d\n", key, num );

    // Get the buffer ready. 10 for the maximum lenghth of an int and +5 for metadata
    char val[ 15 ];

    // looks like: gaugor:333|g
    snprintf( val, sizeof(val), ":%d|g", num );

    _send_to_statsd( priv, key, val );
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

//     _DEBUG && fprintf( stderr, "Open: %.9f Req: %.9f Res: %.9f End: %.9f\n",
//         sp->t_open, sp->t_req, sp->t_resp, sp->t_end );

