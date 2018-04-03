# We just need a container with the go binary installed. Reusing the
# cloud-builder one is the easiest way to do this. (But it can't have the
# entrypoint that the cloud builder one uses.)

FROM gcr.io/cloud-builders/go

ENTRYPOINT ["/bin/sh"]
