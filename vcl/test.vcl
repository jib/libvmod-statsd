### Test reloading of the VCL file and ensure stats are still being
### sent. Assume you're in the same dir as this file.
###
### First, run varnish with this file in the foreground:
### $ varnishd -a :8080 -T :8081 -n . -F -f test.vcl
###
### Next, run a Statsd on port 8225 (different from the standard
### 8125), or run a netcat listener with 'nc -klu 8225'
###
### Then, issue a request and ensure you see a stat show up:
### $ curl -v http://localhost:8080
###
### Then, reload the vcl:
### varnishadm -T :8081 vcl.load reload01 `pwd`/test.vcl
### varnishadm -T :8081 vcl.use reload01
###
### And then issue another request, ensuring stats show up:
### $ curl -v http://localhost:8080

### https://github.com/jib/libvmod-statsd
import std;
import statsd;

### Doesn't matter if nothing is running here.
backend default {
  .host = "127.0.0.1";
  .port = "8082";
}

### If you do not end with a return() statement, all instances of
### vcl_* will be concatenated.
sub vcl_init {
  statsd.server( "127.0.0.1", "8225" );
}

sub vcl_fetch {
  std.log( "fetch" );
  statsd.incr( "fetch" );
}

sub vcl_error {
  std.log( "error" );
  statsd.incr( "error" );

  ### fake response
  set obj.status            = 200;
  set obj.response          = "OK";
  set obj.http.Content-Type = "text/plain";
  synthetic {"response"};

  return(deliver);
}
