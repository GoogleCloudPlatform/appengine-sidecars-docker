package filereceiver

import (
	"context"
	"fmt"
	"time"
	"log"

	"contrib.go.opencensus.io/resource/auto"
	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
	resourcepb "github.com/census-instrumentation/opencensus-proto/gen-go/resource/v1"

	"github.com/golang/protobuf/ptypes/timestamp"
	"github.com/open-telemetry/opentelemetry-collector/consumer"
	"github.com/open-telemetry/opentelemetry-collector/consumer/consumerdata"
)

type FileMetricsCollector struct {
	consumer consumer.MetricsConsumer

	startTime time.Time

	exportInterval time.Duration
	metricName     string
	metricValue    int64
	done           chan struct{}
}

const (
	defaultExportInterval = 1 * time.Minute
)

var rsc *resourcepb.Resource

func NewFileMetricsCollector(exportInterval time.Duration, metricName string, metricValue int64, consumer consumer.MetricsConsumer) *FileMetricsCollector {
	if exportInterval <= 0 {
		exportInterval = defaultExportInterval
	}

	fmc := &FileMetricsCollector{
		consumer:       consumer,
		startTime:      time.Now(),
		metricName:     metricName,
		metricValue:    metricValue,
		exportInterval: exportInterval,
		done:           make(chan struct{}),
	}

	return fmc
}

func detectResource() {
	log.Printf("starting detectResource")
	autoRes, err := auto.Detect(context.Background())
	if err != nil {
		panic(fmt.Sprintf("Resource detection failed, err: %v", err))
	}
	if autoRes != nil {
		rsc = &resourcepb.Resource{
			Type: autoRes.Type,
			Labels: autoRes.Labels,
		}
	}
	log.Printf("ending detectResource")
}

func (fmc *FileMetricsCollector) StartCollection() {
	detectResource()

	go func() {
		log.Printf("in export go function")
		ticker := time.NewTicker(fmc.exportInterval)
		for {
			log.Printf("in export loop")
			select {
			case <-ticker.C:
				log.Printf("before scrapAndExport")
				fmc.scrapeAndExport()
				log.Printf("after scrapeAndExport")
			case <-fmc.done:
				return
			}
		}
	}()
}

func (fmc *FileMetricsCollector) StopCollection() {
	close(fmc.done)
}

func (fmc *FileMetricsCollector) scrapeAndExport() {
	metrics := make([]*metricspb.Metric, 0, 1)
	metrics = append(
		metrics,
		&metricspb.Metric{
			MetricDescriptor: testFileMetric,
			Resource:         rsc,
			Timeseries:       []*metricspb.TimeSeries{fmc.getInt64TimeSeries(fmc.metricValue)},
		},
	)

	ctx := context.Background()
	fmc.consumer.ConsumeMetricsData(ctx, consumerdata.MetricsData{Metrics: metrics})
	log.Printf("Exporting resource")
}

// TimeToTimestamp converts a time.Time to a timestamp.Timestamp pointer.
func TimeToTimestamp(t time.Time) *timestamp.Timestamp {
	if t.IsZero() {
		return nil
	}
	nanoTime := t.UnixNano()
	return &timestamp.Timestamp{
		Seconds: nanoTime / 1e9,
		Nanos:   int32(nanoTime % 1e9),
	}
}

func (vmc *FileMetricsCollector) getInt64TimeSeries(val int64) *metricspb.TimeSeries {
	return &metricspb.TimeSeries{
		StartTimestamp: TimeToTimestamp(vmc.startTime),
		Points:         []*metricspb.Point{{Timestamp: TimeToTimestamp(time.Now()), Value: &metricspb.Point_Int64Value{Int64Value: val}}},
	}
}
