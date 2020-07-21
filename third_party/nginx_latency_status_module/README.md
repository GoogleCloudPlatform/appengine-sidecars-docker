An nginx module that provides a status page similar to the ngx_http_stub_status_module,
but which includes latency distribution stats. The output is in json format.

The store_latency directive needs to be set at the main level of the config
in order for the module's other directives to work.
The latency_stub_status directive sets the location that will serve the status
page, and the record_latency directive sets the location(s) for which latency
stats will be recorded.
