# Dockerfile extending the debian8 image to run iap_verifier.py to check
# gae_full_app_container is available.

FROM gcr.io/google_appengine/debian8

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends python python-pip && \
    apt-get clean
RUN pip install google-compute-engine

RUN mkdir -p /home/vmagent/iap_verifier
WORKDIR /home/vmagent/iap_verifier
ADD iap_verifier.py .
ENTRYPOINT ["./iap_verifier.py"]

