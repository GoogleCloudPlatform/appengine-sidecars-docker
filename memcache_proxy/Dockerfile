FROM gcr.io/google_appengine/base

COPY memcachep /home/memcache/
WORKDIR /home/memcache
EXPOSE 11211
ENTRYPOINT ["/home/memcache/memcachep", "-binding_address=0.0.0.0:11211"]
