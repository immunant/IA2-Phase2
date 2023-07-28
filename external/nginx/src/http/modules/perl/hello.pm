package hello;

use nginx;
# Enables the mnemonic for PID
use English;

sub handler {
        my $r = shift;

        $r->send_http_header("text/html");
        return OK if $r->header_only;

        $r->print("hello from perl! my PID is $PID.\n");

        return OK;
    }

1;
__END__
