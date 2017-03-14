# Dockerfile extending the generic Python image to run an metadata watcher app.

FROM gcr.io/google_appengine/python
WORKDIR /home/vmagent/metadata_watcher
ADD metadata_watcher.py .
ENTRYPOINT ["./metadata_watcher.py"]

