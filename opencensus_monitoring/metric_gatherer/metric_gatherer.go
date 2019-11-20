// Definitions of measures and utility functions to record them.
package metric_gatherer

import (
	"context"
	"log"

	//"google3/third_party/golang/opencensus/stats/stats"
	"go.opencensus.io/stats"
	"go.opencensus.io/stats/view"
	"go.opencensus.io/tag"
)

type metricRegistrar interface {
	register() bool
	getTagMutators(map[string]string) []tag.Mutator
}

type metric struct {
	Name string
	Description string
	Unit string
	TagNames []string

	measure stats.Measure
	aggregation *view.Aggregation
	tagKeys []tag.Key
}

type IntMetric struct {
	metric
	intMeasure *stats.Int64Measure
}

type FloatMetric struct {
	metric
	floatMeasure *stats.Float64Measure
}

func newFloatDistributionMetric(name, description, unit string, tagNames []string, bounds []float64) *FloatMetric {
	measure := stats.Float64(name, description, unit)
	return &FloatMetric{
		metric: metric{
			Name: name,
			Description: description,
			Unit: unit,
			TagNames: tagNames,
			measure: measure,
			aggregation: view.Distribution(bounds...),
		},
		floatMeasure: measure,
	}
}

func newCountMetric(name, description, unit string, tagNames []string) *IntMetric {
	measure := stats.Int64(name, description, unit)
	return &IntMetric{
		metric: metric{
			Name: name,
		Description: description,
		Unit: unit,
		TagNames: tagNames,
		measure: measure,
		aggregation: view.Count(),
	},
		intMeasure: measure,
	}
}

func (m *metric) register() bool {
        for _, name := range m.TagNames {
                key, err := tag.NewKey(name)
                if err != nil {
                        log.Printf("Failed to set the label %v for view %v: %v", name, m.Name, err)
                        return false
                }
                m.tagKeys = append(m.tagKeys, key)
        }

        distributionView := &view.View{
                Name:        m.measure.Name(),
                Measure:     m.measure,
                Description: m.measure.Description(),
                Aggregation: m.aggregation,
                TagKeys: m.tagKeys,
        }

        err := view.Register(distributionView)
        if err != nil {
                log.Printf("Failed to register the view %v: %v", m.measure.Name(), err)
        }
        return err == nil
}

func (m *metric) getTagMutators(labels map[string]string) []tag.Mutator {
	var mutators []tag.Mutator

	for _, tagKey := range m.tagKeys {
		value, present := labels[tagKey.Name()]
		if present {
			mutators = append(mutators, tag.Insert(tagKey, value))
		} else {
			mutators = append(mutators, tag.Insert(tagKey, "unset"))
		}
	}
	return mutators
}

func (m *IntMetric) recordInt(value int64, labels map[string]string) {
	ctx := context.Background()
	stats.RecordWithTags(ctx, m.getTagMutators(labels), m.intMeasure.M(value))
}

func (m *FloatMetric) recordFloat(value float64, labels map[string]string) {
	ctx := context.Background()
	stats.RecordWithTags(ctx, m.getTagMutators(labels), m.floatMeasure.M(value))
}

