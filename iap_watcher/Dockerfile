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
ADD iap_watcher.py .
ENV OUTPUT_KEY_FILE /iap_watcher/iap_verify_keys.txt
ENTRYPOINT [ \
    "./iap_watcher.py", \
    "--iap_metadata_key=AEF_IAP_state", \
    "--output_state_file=/iap_watcher/iap_state", \
    "--fetch_keys=True", \
    "--output_key_file=${OUTPUT_KEY_FILE}"]

RUN ["/bin/bash", "-c", "echo \"$(($RANDOM%60)) * * * * curl 'https://www.gstatic.com/iap/verify/public_key-jwk' > ${OUTPUT_KEY_FILE}\" >> .tmp_cron"]
RUN crontab .tmp_cron
RUN service cron start
RUN rm .tmp_cron
