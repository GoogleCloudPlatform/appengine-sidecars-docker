# Dockerfile extending the debian8 image to run an metadata watcher app.

FROM gcr.io/google_appengine/debian8

RUN apt-get update
RUN apt-get install -y --no-install-recommends python python-pip
RUN apt-get clean
RUN pip install google-compute-engine

WORKDIR /home/vmagent/metadata_watcher
ADD metadata_watcher.py .
ENTRYPOINT ["./metadata_watcher.py"]

