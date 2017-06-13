# Dockerfile extending the debian8 image to run iap_watcher.py to auto
# update IAP state and verifier keys.

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

RUN crontab -l > .tmp_cron
RUN echo "? */12 * * * * curl https://www.gstatic.com/iap/verify/public_key > /iap_watcher/iap_verify_keys.txt" >> .tmp_cron
RUN crontab .tmp_cron
RUN rm .tmp_cron