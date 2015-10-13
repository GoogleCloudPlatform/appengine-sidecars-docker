#!/bin/bash

export GOPATH=$(mktemp --directory)
mkdir -p $GOPATH/src $GOPATH/pkg

go get google.golang.org/appengine/internal
cp -R $GOPATH/src/google.golang.org/appengine/internal $GOPATH/src/google.golang.org/appengine/notreallyinternal
sed -i 's|instance/attributes/gae_minor_version|instance/attributes/gae_backend_minor_version|g' $GOPATH/src/google.golang.org/appengine/notreallyinternal/identity_vm.go

DEST=$GOPATH/src

echo "Building in $DEST"

mkdir -p $DEST/dtog/main
cp dtog.go $DEST/dtog
cp dtog_test.go $DEST/dtog
cp main/memcached2g.go $DEST/dtog/main

go build -o memcachep main/memcached2g.go
