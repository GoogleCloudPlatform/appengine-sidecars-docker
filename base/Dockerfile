FROM gcr.io/google-appengine/debian8
# Clean any CMD that might be inherited from previous image, because that
# will pollute our ENTRYPOINT, see
# http://docs.docker.io/en/latest/reference/builder/#entrypoint.
CMD []
ENV DEBIAN_FRONTEND noninteractive
ENV PORT 8080
RUN apt-get -q update && \
    apt-get install --no-install-recommends -y -q ca-certificates && \
    apt-get -y -q upgrade && \
    rm /var/lib/apt/lists/*_*
