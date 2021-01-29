# Stackdriver monitoring agent that collects metrics and send to Stackdriver.

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian10:${BASE_IMAGE_TAG}

# From https://cloud.google.com/monitoring/agent/installation#joint-install
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    gnupg2 \
    && curl -sSO https://dl.google.com/cloudagents/add-monitoring-agent-repo.sh \
    && bash /add-monitoring-agent-repo.sh \
    && apt-get update \
    && apt-get install -y stackdriver-agent \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*_* \
    && rm -f /add-monitoring-agent-repo.sh

# Install OpenJDK-11
RUN apt-get update && \
    apt-get install -y openjdk-11-jdk && \
    apt-get install -y ant && \
    apt-get clean;

ENV JAVA_HOME /usr/lib/jvm/java-11-openjdk-amd64
RUN export JAVA_HOME

# Link path so Collectd can use Java plugin.
ENV LD_LIBRARY_PATH=$JAVA_HOME/lib/server:$LD_LIBRARY_PATH
RUN export LD_LIBRARY_PATH

# Allow user specified configuration files.
VOLUME ["/etc/collectd/collectd.d/"]

ADD collectd.conf /etc/collectd/collectd.conf
ADD run.sh /run.sh

CMD /run.sh
