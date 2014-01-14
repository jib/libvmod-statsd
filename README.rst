============
vmod_statsd
============

----------------------
Varnish Statsd Module
----------------------

:Author: Jos Boumans
:Date: 2014-01-14
:Version: 1.1
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

To install this module, you'll need to install some prerequisites. On Ubuntu,
you can get these by running::

 $ apt-get install automake libtool python-docutils

You will also need a compiled version of the varnish source code, which you
can get from here:

 https://www.varnish-cache.org
 
The compilation of varnish is similar to this package. Please refer to the
varnish documentation for all the options, but briefly, it is::

 $ ./autogen.sh
 $ ./configure
 $ make

If you received this packge without a pre-generated configure script, you will
have to generate it using 'autogen.sh'. Otherwise, you can move straight on to
the 'configure' section under Usage.

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
* http://jiboumans.wordpress.com/2013/02/27/realtime-stats-from-varnish/
* https://gist.github.com/jib/5034755

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-statsd project. See LICENSE for details.

* Copyright (c) 2012 Jos Boumans
