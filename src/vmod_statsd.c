#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

// Socket related libraries
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

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

    //_DEBUG && fprintf( stderr, "vmod-statsd: stripping new lines. New string: %s\n", line );


    return line;
}

// ******************************
// Configuration
// ******************************

static void
free_function(void *priv) {
    config_t *cfg = priv;
    if( cfg->socket > 0 ) {
        _DEBUG && fprintf( stderr, "vmod-statsd: free: Closing socket with FD %d\n", cfg->socket );
        int close_ret = close( cfg->socket );
        if( close_ret != 0 ) {
            int close_error = errno;
            _DEBUG && fprintf( stderr, "vmod-statsd: free: Error closing socket: %s (errno %d)\n",
                             strerror(close_error), close_error );
        }
        cfg->socket = 0;
        _DEBUG && fprintf( stderr, "vmod-statsd: free: Socket closed/reset\n" );
    }
}

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
    cfg->socket = 0;

    _DEBUG && fprintf( stderr, "vmod-statsd: init: configuration initialized\n" );

    // ******************************
    // Store the config
    // ******************************

    priv->priv = cfg;
    priv->free = free_function;

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
    struct addrinfo *statsd; /* will allocated by getaddrinfo */
    struct addrinfo *hints;
    hints = malloc(sizeof(struct addrinfo));

    if (hints == NULL) {
        fprintf( stderr, "vmod-statsd: malloc failed for hints addrinfo struct\n"  );
        return -1;
    }

    // Hints can be full of garbage, and it's lead to segfaults inside
    // Varnish. See this issue: https://github.com/jib/libvmod-statsd/pull/5/files
    // As a way to fix that, we zero out the memory, as also pointed out
    // in the freeaddrinfo manpage: http://linux.die.net/man/3/freeaddrinfo
    memset( hints, 0, sizeof(struct addrinfo) );

    // what type of socket is the statsd endpoint?
    hints->ai_family   = AF_UNSPEC;
    hints->ai_socktype = SOCK_DGRAM;
    hints->ai_protocol = IPPROTO_UDP;
    hints->ai_flags    = 0;

    _DEBUG && fprintf( stderr, "vmod-statsd: socket hints compiled\n" );

    // using getaddrinfo lets us use a hostname, rather than an
    // ip address.
    int err;
    if( (err = getaddrinfo( cfg->host, cfg->port, hints, &statsd )) != 0 ) {
        fprintf( stderr, "vmod-statsd: getaddrinfo on %s:%s failed: %s\n",
            cfg->host, cfg->port, gai_strerror(err) );

        freeaddrinfo( hints );
        return -1;
    }

    _DEBUG && fprintf( stderr, "vmod-statsd: getaddrinfo completed\n" );

    // ******************************
    // Store the open connection
    // ******************************

    // getaddrinfo() may return more than one address structure
    // but since this is UDP, we can't verify the connection
    // anyway, so we will just use the first one
    cfg->socket = socket( statsd->ai_family, statsd->ai_socktype,
                       statsd->ai_protocol );

    if( cfg->socket == -1 ) {
        _DEBUG && fprintf( stderr, "vmod-statsd: socket creation failed\n" );
        close( cfg->socket );
        freeaddrinfo( statsd );
        freeaddrinfo( hints );
        return -1;
    }

    _DEBUG && fprintf( stderr, "vmod-statsd: socket opened: %d\n", cfg->socket );

    // connection failed.. for some reason...
    if( connect( cfg->socket, statsd->ai_addr, statsd->ai_addrlen ) == -1 ) {
        _DEBUG && fprintf( stderr, "vmod-statsd: socket connection failed\n" );
        close( cfg->socket );

        freeaddrinfo( statsd );
        freeaddrinfo( hints );
        return -1;
    }

    _DEBUG && fprintf( stderr, "vmod-statsd: socket connected\n" );

    // now that we have an outgoing socket, we don't need this
    // anymore.
    freeaddrinfo( statsd );
    freeaddrinfo( hints );

    _DEBUG && fprintf( stderr, "vmod-statsd: statsd server: %s:%s (fd: %d)\n",
                cfg->host, cfg->port, cfg->socket );

    return cfg->socket;
}

int
_send_to_statsd( struct vmod_priv *priv, const char *key, const char *val ) {
    _DEBUG && fprintf( stderr, "vmod-statsd: pre config\n" );

    config_t *cfg = priv->priv;

    _DEBUG && fprintf( stderr, "vmod-statsd: post config\n" );

    // If you are using some empty key, bail - this can happen if you use
    // say: statsd.incr( req.http.x-does-not-exist ). Rather than getting
    // and empty string, we get a null pointer.
    if( key == NULL || val == NULL ) {
        _DEBUG && fprintf( stderr, "vmod-statsd: Key or value is NULL pointer - ignoring\n" );
        return -1;
    }

    _DEBUG && fprintf( stderr, "vmod-statsd: pre stat composition\n" );

    // Enough room for the key/val plus prefix/suffix plus newline plus a null byte.
    char stat[ strlen(key) + strlen(val) +
               strlen(cfg->prefix) + strlen(cfg->suffix) + 1 ];

    strncpy( stat, cfg->prefix, strlen(cfg->prefix) + 1 );
    strncat( stat, key,         strlen(key)         + 1 );
    strncat( stat, cfg->suffix, strlen(cfg->suffix) + 1 );
    strncat( stat, val,         strlen(val)         + 1 );

    _DEBUG && fprintf( stderr, "vmod-statsd: post stat composition: %s\n", stat );

    // Newer versions of statsd allow multiple metrics in a single packet, delimited
    // by newlines. That unfortunately means that if we end our message with a new
    // line, statsd will interpret this as an empty second metric and log a 'bad line'.
    // This is true in at least version 0.5.0 and to avoid that, we don't send the
    // newline. Makes debugging using nc -klu 8125 a bit more tricky, but works with
    // modern statsds.
    //strncat( stat, "\n",        1                       );

    //_DEBUG && fprintf( stderr, "vmod-statsd: send: %s:%s %s\n", cfg->host, cfg->port, stat );

    // ******************************
    // Sanity checks
    // ******************************

    _DEBUG && fprintf( stderr, "vmod-statsd: pre stat length\n" );

    int len = strlen( stat );

    _DEBUG && fprintf( stderr, "vmod-statsd: stat length: %d\n", len );

    // +1 for the null byte
    if( len + 1 >= BUF_SIZE ) {
        _DEBUG && fprintf( stderr, "vmod-statsd: Message length %d > max length %d - ignoring\n",
            len, BUF_SIZE );
        return -1;
    }

    // ******************************
    // Send the packet
    // ******************************

    _DEBUG && fprintf( stderr, "vmod-statsd: Checking for existing socket (%d)\n", cfg->socket );

    // we may not have connected yet - in that case, do it now
    int sock = cfg->socket > 0 ? cfg->socket : _connect_to_statsd( priv );

    // If we didn't get a socket, don't bother trying to send
    if( sock == -1 ) {
        _DEBUG && fprintf( stderr, "vmod-statsd: Could not get socket for %s\n", stat );
        return -1;
    }

    // Send the stat
    int sent = write( sock, stat, len );

    _DEBUG && fprintf( stderr, "vmod-statsd: Sent %d of %d bytes to FD %d\n", sent, len, sock );

    // An error occurred - unset the socket so that the next write may try again
    if( sent != len ) {
        int write_error = errno;
        _DEBUG && fprintf( stderr, "vmod-statsd: Could not write stat '%s': %s (errno %d)\n",
                         stat, strerror(write_error), write_error );

        // if the write_error is not due to a bad file descriptor, try to close the socket first
        if( write_error != 9 ) {
            _DEBUG && fprintf( stderr, "vmod-statsd: Closing socket with FD: %d\n", sock );
            int close_ret_val = close( sock );
            if( close_ret_val != 0 ) {
                int close_error = errno;
                _DEBUG && fprintf( stderr, "vmod-statsd: Error closing socket: %s (errno %d)\n",
                                 strerror(close_error), close_error );
            }
        }
        // reset the socket
        cfg->socket = 0;
        _DEBUG && fprintf( stderr, "vmod-statsd: Socket closed/reset\n" );

        return -1;
    }

    return 0;
}


void
vmod_incr( struct sess *sp, struct vmod_priv *priv, const char *key ) {
    _DEBUG && fprintf( stderr, "vmod-statsd: incr: %s\n", key );

    // Incremenet is straight forward - just add the count + type
    _send_to_statsd( priv, key, ":1|c" );
}

void
vmod_timing( struct sess *sp, struct vmod_priv *priv, const char *key, int num ) {
    _DEBUG && fprintf( stderr, "vmod-statsd: timing: %s = %d\n", key, num );

    // Get the buffer ready. 10 for the maximum lenghth of an int and +5 for metadata
    char val[ 15 ];

    // looks like glork:320|ms
    snprintf( val, sizeof(val), ":%d|ms", num );

    _send_to_statsd( priv, key, val );
}

void
vmod_counter( struct sess *sp, struct vmod_priv *priv, const char *key, int num ) {
    _DEBUG && fprintf( stderr, "vmod-statsd: counter: %s = %d\n", key, num );

    // Get the buffer ready. 10 for the maximum lenghth of an int and +5 for metadata
    char val[ 15 ];

    // looks like: gorets:42|c
    snprintf( val, sizeof(val), ":%d|c", num );

    _send_to_statsd( priv, key, val );
}

void
vmod_gauge( struct sess *sp, struct vmod_priv *priv, const char *key, int num ) {
    _DEBUG && fprintf( stderr, "vmod-statsd: gauge: %s = %d\n", key, num );

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

//     _DEBUG && fprintf( stderr, "vmod-statsd: Open: %.9f Req: %.9f Res: %.9f End: %.9f\n",
//         sp->t_open, sp->t_req, sp->t_resp, sp->t_end );

