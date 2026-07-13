to get logging in the nwetworking part there are 2 ways

RUDP_LOG=1 ./server 8080
RUDP_LOG=1 ./client 127.0.0.1 8080

OR
we
export RUDP_LOG=1 and then run the binaries

export DEV_LOG=1 for debugging
