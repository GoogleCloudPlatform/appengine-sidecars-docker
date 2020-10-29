# Stackdriver monitoring agent that collects metrics and send to Stackdriver.

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian9:${BASE_IMAGE_TAG}

# From https://cloud.google.com/monitoring/agent/install-agent
ADD https://dl.google.com/cloudagents/install-monitoring-agent.sh /install-monitoring-agent.sh
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    gnupg2 \
    && bash /install-monitoring-agent.sh \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*_*

# Install OpenJDK-8
RUN apt-get update && \
    apt-get install -y openjdk-8-jdk && \
    apt-get install -y ant && \
    apt-get clean;

# Fix certificate issues
RUN apt-get update && \
    apt-get install ca-certificates-java && \
    apt-get clean && \
    update-ca-certificates -f;

ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64
RUN export JAVA_HOME

# Link path so Collectd can use Java plugin.
ENV LD_LIBRARY_PATH=$JAVA_HOME/jre/lib/amd64/server:$LD_LIBRARY_PATH
RUN export LD_LIBRARY_PATH

# Allow user specified configuration files.
VOLUME ["/etc/collectd/collectd.d/"]

ADD collectd.conf /etc/collectd/collectd.conf
ADD run.sh /run.sh

CMD /run.sh
