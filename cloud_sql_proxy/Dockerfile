FROM scratch

COPY cloud_sql_proxy /cloud_sql_proxy
ENTRYPOINT ["/cloud_sql_proxy", "-dir=/cloudsql", "-instances_metadata=instance/attributes/gae_cloud_sql_instances", "-check_region"]
