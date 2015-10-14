#!/bin/bash
set -e

echo "Preparing build ..."
export GOPATH=$(mktemp --directory)
mkdir -p $GOPATH/src $GOPATH/pkg

go get google.golang.org/appengine/internal
cp -R $GOPATH/src/google.golang.org/appengine/internal $GOPATH/src/google.golang.org/appengine/notreallyinternal
sed -i 's|instance/attributes/gae_minor_version|instance/attributes/gae_backend_minor_version|g' $GOPATH/src/google.golang.org/appengine/notreallyinternal/identity_vm.go
sed -i 's|"google\.golang\.org/appengine/internal"|internal "google\.golang\.org/appengine/notreallyinternal"|g' $GOPATH/src/google.golang.org/appengine/notreallyinternal/aetesting/fake.go

DEST=$GOPATH/src

echo "Building in $DEST"

mkdir -p $DEST/dtog/main
cp dtog.go $DEST/dtog
cp dtog_test.go $DEST/dtog
cp main/memcached2g.go $DEST/dtog/main

go build -o memcachep main/memcached2g.go

echo "Testing in $DEST"
go test $DEST/dtog/dtog.go $DEST/dtog/dtog_test.go

echo "Done, you binary is here ./memcachep"

