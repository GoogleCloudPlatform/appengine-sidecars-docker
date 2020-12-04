go_library(
    name = "vmagereceiver",
    srcs = ["config.go"],
)

go_test(
    name = "vmagereceiver_test",
    size = "small",
    srcs = ["config_test.go"],
    library = ":vmagereceiver",
)

go_library(
    name = "vmagereceiver",
    srcs = [
        "config.go",
        "doc.go",
        "factory.go",
        "metrics_receiver.go",
        "vm_age_collector.go",
        "vm_age_constants.go",
    ],
)

go_test(
    name = "vmagereceiver_test",
    size = "small",
    srcs = [
        "config_test.go",
        "factory_test.go",
        "vm_age_collector_test.go",
    ],
    library = ":vmagereceiver",
)
