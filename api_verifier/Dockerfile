# Dockerfile extending the generic Python image to run an API verification app.
# TO RUN:
#   docker run [IMAGE_NAME, e.g. google/appengine-api-verifier]
#
# Possible environment variable:
# DEFAULT_TICKET: The security ticket for API calls.  If not given this will
#                 be constructed via the standard environment variables or
#                 metadata.

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/python-compat:${BASE_IMAGE_TAG}
ADD . /home/vmagent/api_verifier
WORKDIR /home/vmagent/api_verifier
ENTRYPOINT ["./api_verifier.py"]

