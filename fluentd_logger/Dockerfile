# To run:
# docker run -d -v /var/log:/var/log:rw -v /var/lib/docker:/var/lib/docker:rw
FROM gcr.io/google_appengine/base
RUN apt-get -q update && \
    apt-get -y install apt-utils adduser ca-certificates curl lsb-release build-essential && \
    apt-get clean && \
    rm /var/lib/apt/lists/*_*

RUN curl -s https://storage.googleapis.com/signals-agents/logging/google-fluentd-install.sh | DO_NOT_INSTALL_CATCH_ALL_CONFIG=1 bash

RUN /usr/sbin/google-fluentd-gem install --conservative fluent-plugin-detect-exceptions:0.0.4

# Add cloud agent driver
ADD out_from_docker.rb /etc/google-fluentd/plugin/out_from_docker.rb

ADD managed_vms.conf /etc/google-fluentd/google-fluentd.conf

CMD []

ENTRYPOINT ["/opt/google-fluentd/embedded/bin/ruby", \
            "/usr/sbin/google-fluentd", \
            "--log", "/var/log/google-fluentd/google-fluentd.log"]
