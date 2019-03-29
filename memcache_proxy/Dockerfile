ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian9:${BASE_IMAGE_TAG}

COPY memcachep /home/memcache/
WORKDIR /home/memcache
EXPOSE 11211
ENTRYPOINT ["/home/memcache/memcachep", "-binding_address=0.0.0.0:11211"]
