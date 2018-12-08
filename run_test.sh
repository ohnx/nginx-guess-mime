#!/bin/bash

PATHROOT=$(pwd)
TESTROOT=$PATHROOT/nginx-tests
mkdir -p $PATHROOT/nginx-tests

$TESTROOT/nginx -s stop

cd nginx
./configure --add-module=$PATHROOT --prefix=$PATHROOT/nginx-tests --conf-path=$TESTROOT/test.conf --http-log-path=$TESTROOT/access.log --error-log-path=$TESTROOT/error.log --with-debug

make -j 4

cp objs/nginx $TESTROOT

cd ../nginx-tests

cat <<JIL >test.conf
error_log $TESTROOT/error.log debug;

events {
    worker_connections 768;
}

http {
    guess_mime on;

    server {
        listen 9005;
        root $TESTROOT;

        location / {
            guess_mime off;
        }
    }

    server {
        listen 9006;
        root $TESTROOT;

        guess_mime on;
    }
}


JIL

./nginx

cd $PATHROOT;

echo "run to stop:"
echo "$TESTROOT/nginx -s stop"

tail -f $TESTROOT/error.log
