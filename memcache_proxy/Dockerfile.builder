# We just need a container with the go binary installed. I figured reusing the
# cloud-builder one is the best way to go. (But it can't have the entrypoint
# that the cloud builder one uses.)

FROM gcr.io/cloud-builders/go

ENTRYPOINT ["/bin/sh"]
