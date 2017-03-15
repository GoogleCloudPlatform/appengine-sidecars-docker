# Dockerfile extending the generic Python image to run an metadata watcher app.

FROM gcr.io/google_appengine/debian8
RUN apt-get update
RUN apt-get install -y --no-install-recommends python
RUN apt-get clean
WORKDIR /home/vmagent/metadata_watcher
ADD metadata_watcher.py .
ENTRYPOINT ["./metadata_watcher.py"]

