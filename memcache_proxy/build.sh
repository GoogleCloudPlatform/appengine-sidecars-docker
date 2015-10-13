#!/bin/bash

export GOPATH=$(mktemp --directory)
mkdir -p $GOPATH/src $GOPATH/pkg

go get google.golang.org/appengine/internal
sed -i 's|instance/attributes/gae_minor_version|instance/attributes/gae_backend_minor_version|g' $GOPATH/src/google.golang.org/appengine/internal/identity_vm.go
cp -R $GOPATH/src/google.golang.org/appengine/internal $GOPATH/src/google.golang.org/appengine/notreallyinternal

DEST=$GOPATH/src
mkdir -p $DEST/dtog/main
cp dtog.go $DEST/dtog
cp main/memcached2g.go $DEST/dtog/main

go build -o memcachep main/memcached2g.go
