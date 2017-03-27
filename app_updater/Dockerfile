# Dockerfile extending the debian8 image to run app_updater.py to check
# gae_full_app_container is available.

FROM gcr.io/google_appengine/debian8

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends python python-pip && \
    apt-get clean
RUN pip install google-compute-engine

RUN mkdir -p /home/vmagent/app_updater
WORKDIR /home/vmagent/app_updater
ADD app_updater.py .
ENTRYPOINT ["./app_updater.py"]

