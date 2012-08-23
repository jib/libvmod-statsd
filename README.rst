============
vmod_statsd
============

----------------------
Varnish Statsd Module
----------------------

:Author: Jos Boumans
:Date: 2012-08-22
:Version: 1.0
:Manual section: 3

SYNOPSIS
========

    import statsd;

    sub vcl_init {
        # Optional, defaults to localhost:8125
        statsd.server( "statsd.example.com", "8125" );
    }

    sub vcl_deliver {
        statsd.incr(    "incr"          );
        statsd.gauge(   "gauge",    42  );
        statsd.timing(  "timing",   42  );
        statsd.counter( "counter",  42  );
    }

DESCRIPTION
===========

Varnish Module (vmod) for sending statistics to Statsd.

See https://github.com/etsy/statsd for documentation on Statsd.

FUNCTIONS
=========

server
------

Prototype::

                server(STRING S, STRING S)

Return value
	NONE
Description
	Set the address of your Statsd server.
    Best used in vcl_init. Defaults to "localhost", "8125"

Example::

                statsd.server( "statsd.example.com", "8125" );

prefix
------

Prototype::

                prefix(STRING S)

Return value
	NONE
Description
	Set a string to prefix to all the stats you will be sending.
    Best used in vcl_init. Defaults to an empty string.

Example::

                # statsd.incr( "foo" ) will now send "dev.foo" to Statsd
                statsd.prefix( "dev." );

suffix
------

Prototype::

                suffix(STRING S)

Return value
	NONE
Description
	Set a string to suffix to all the stats you will be sending.
	Best used in vcl_init. Defaults to an empty string.

Example::

                # statsd.incr( "foo" ) will now send "foo.dev" to Statsd
                statsd.suffix( ".dev" );

incr
----

Prototype::

                incr(STRING S)

Return value
	NONE
Description
	Send a stat counter with value '1' to Statsd. Will be prefixed & suffixed
	with whatever you set statsd.prefix & statsd.suffix to.

Example::

                statsd.incr( "foo" );

counter
-------

Prototype::

                counter(STRING S, INT I)

Return value
	NONE
Description
	Send a stat counter with value I to Statsd. Will be prefixed & suffixed
	with whatever you set statsd.prefix & statsd.suffix to.

Example::

                statsd.counter( "foo", 42 );

timing
-------

Prototype::

                timing(STRING S, INT I)

Return value
	NONE
Description
	Send a stat timer with value I to Statsd. Will be prefixed & suffixed
	with whatever you set statsd.prefix & statsd.suffix to.

Example::

                statsd.timing( "foo", 42 );

gauge
-----

Prototype::

                gauge(STRING S, INT I)

Return value
	NONE
Description
	Send a stat gauge with value I to Statsd. Will be prefixed & suffixed
	with whatever you set statsd.prefix & statsd.suffix to.

Example::

                statsd.gauge( "foo", 42 );


INSTALLATION
============

If you received this packge without a pre-generated configure script, you must
have the GNU Autotools installed, and can then run the 'autogen.sh' script. If
you received this package with a configure script, skip to the second
command-line under Usage to configure.

Usage::

 # Generate configure script
 ./autogen.sh

 # Execute configure script
 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod. Both the `VARNISHSRC` and `VARNISHSRC/include`
will be added to the include search paths for your module.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``


SEE ALSO
========

* https://github.com/etsy/statsd
* https://www.varnish-cache.org

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-statsd project. See LICENSE for details.

* Copyright (c) 2012 Jos Boumans
