FROM gcr.io/google-appengine/debian9:latest

# From https://cloud.google.com/monitoring/agent/install-agent
ADD https://dl.google.com/cloudagents/install-monitoring-agent.sh /install-monitoring-agent.sh

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    gnupg2 \
    && bash /install-monitoring-agent.sh \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*_*

# Allow user specified configuration files.
VOLUME ["/etc/collectd/collectd.d/"]

ADD collectd.conf /etc/collectd/collectd.conf
ADD run.sh /run.sh

CMD /run.sh
