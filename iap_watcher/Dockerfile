# Dockerfile extending the debian10 image to run iap_watcher.py to auto
# update IAP state and verifier keys.

#
# Run with:
#   gcloud builds submit --config cloudbuild.yaml . \
#     --substitutions=_RC_NAME=20180101-RC00


ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian10:${BASE_IMAGE_TAG} AS iap-watcher

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends cron curl python3 python3-pip python3-setuptools && \
    apt-get clean

RUN pip3 install wheel && \
    pip3 install google-compute-engine

RUN mkdir /iap_watcher
VOLUME /iap_watcher

RUN mkdir -p /home/vmagent/iap_watcher
WORKDIR /home/vmagent/iap_watcher
ADD start_iap_watcher.sh .
RUN chmod +x ./start_iap_watcher.sh
ADD iap_watcher.py .

ENTRYPOINT ["./start_iap_watcher.sh"]

FROM iap-watcher as iap-watcher-tester

RUN pip3 install jsobject
ADD iap_watcher_test.py .

ENTRYPOINT ["python3", "iap_watcher_test.py"]
