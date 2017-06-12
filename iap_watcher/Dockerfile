# Dockerfile extending the debian8 image to run iap_watcher.py to check
# gae_full_app_container is available.

FROM gcr.io/google_appengine/debian8

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends python python-pip && \
    apt-get clean
RUN pip install google-compute-engine

RUN mkdir /iap_watcher
VOLUME /iap_watcher

RUN mkdir -p /home/vmagent/iap_watcher
WORKDIR /home/vmagent/iap_watcher
ADD iap_watcher.py .
ENTRYPOINT ["./iap_watcher.py", "--key=AEF_IAP_state", "--output_state_file=/iap_watcher/state.json"]
