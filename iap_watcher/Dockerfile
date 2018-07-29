# Dockerfile extending the debian8 image to run iap_watcher.py to auto
# update IAP state and verifier keys.

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian8:${BASE_IMAGE_TAG}

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends cron curl python python-pip && \
    apt-get clean
RUN pip install google-compute-engine

RUN mkdir /iap_watcher
VOLUME /iap_watcher

RUN mkdir -p /home/vmagent/iap_watcher
WORKDIR /home/vmagent/iap_watcher
ADD startup_script.sh .
ADD iap_watcher.py .
ENTRYPOINT ["./startup_script.sh"]

