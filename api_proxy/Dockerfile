# There are some options to build:
# Option 1: Build the static binary by hand:
#   $ GOOS=linux GOARCH=amd64 go build -o api_proxy-linux-amd64 .
#   $ docker build . gcr.io/${DOCKER_NAMESPACE}/api-proxy
# Option 2: Use a tool like golang-builder to build for you:
#   $ docker run --rm -e BUILD_GOOS="linux" -e BUILD_GOARCH="amd64" \
#       -v $(pwd):/src -v /var/run/docker.sock:/var/run/docker.sock \
#       centurylink/golang-builder-cross gcr.io/$DOCKER_NAMESPACE/api-proxy
FROM scratch
COPY api_proxy-linux-amd64 /proxy
ENTRYPOINT ["/proxy"]
