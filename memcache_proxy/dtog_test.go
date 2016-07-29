/*
Copyright 2015 Google Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

package dtog

import (
	"bufio"
	"fmt"
	"log"
	"net"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/golang/protobuf/proto"
	netcontext "golang.org/x/net/context"
	"google.golang.org/appengine/notreallyinternal/aetesting"
	pb "google.golang.org/appengine/notreallyinternal/memcache"
)

const (
	// These are sentinels placed in the stream of responses.
	timeoutMarker = "-----timeout-----"
	eofMarker     = "-----eof-----"

	// Binding Addr for testing purposes.
	bindingAddr = "localhost:11211"
)

func generateString(n int, r rune) string {
	b := make([]rune, n)
	for i := range b {
		b[i] = r
	}
	return string(b)
}

var maxMemcachePayloadValue = generateString(1000000, 'a')

// Read lines from connection, folding in errors with responses and
// returning special markers for timeout and EOF.
func readLines(conn net.Conn, n int) (lines []string) {
	r := bufio.NewReader(conn)
	for i := 0; i < n-1; i++ {
		line, err := r.ReadString('\n')
		if err != nil {
			line = err.Error()
		}
		lines = append(lines, line)
	}
	bytes, err := r.Peek(100)
	var line string
	if err == nil {
		line = fmt.Sprintf("%q", bytes)
	} else if strings.Contains(err.Error(), "timeout") {
		line = timeoutMarker
	} else if err.Error() == "EOF" {
		line = eofMarker
	} else {
		line = err.Error()
	}
	lines = append(lines, line)
	return
}

// sendCommand is the main helper used by all the Test*() functions
// below. It starts the proxy server, opens a connection to it, sends
// a memcached command, and reads the response.
func sendCommand(ctx netcontext.Context, command string, expectedLines int) []string {
	// Start the proxy server running
	proxy, err := StartAsync(ctx, bindingAddr)
	if err != nil {
		log.Printf("%s -- retrying", err)
		time.Sleep(time.Second)
		proxy, err = StartAsync(ctx, bindingAddr)
		if err != nil {
			log.Fatal(err)
		}
	}
	defer func() {
		if err := proxy.Close(); err != nil {
			log.Fatal(err)
		}
	}()

	// Open connection to the proxy.
	conn, err := net.Dial("tcp", proxy.BindingAddr)
	if err != nil {
		log.Fatal(err)
	}
	conn.SetReadDeadline(time.Now().Add(10 * time.Millisecond))

	// Send a blank line
	fmt.Fprintf(conn, command)

	// read and return the response
	return readLines(conn, expectedLines)
}

// Instead of the real App Engine context, we use a fake one that
// handles a single get request using the callback f().
func fakeGetContext(t *testing.T, f func(req *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse)) netcontext.Context {
	return aetesting.FakeSingleContext(t, "memcache", "Get", func(req *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse) error {
		f(req, res)
		return nil
	})
}

var getNeverCalled = fakeGetContext(nil, func(_ *pb.MemcacheGetRequest, _ *pb.MemcacheGetResponse) {
	log.Fatal("service unexpectedly called")
})

func fakeDeleteContext(t *testing.T, f func(req *pb.MemcacheDeleteRequest, res *pb.MemcacheDeleteResponse)) netcontext.Context {
	return aetesting.FakeSingleContext(t, "memcache", "Delete", func(req *pb.MemcacheDeleteRequest, res *pb.MemcacheDeleteResponse) error {
		f(req, res)
		return nil
	})
}

func fakeIncrContext(t *testing.T, f func(req *pb.MemcacheIncrementRequest, res *pb.MemcacheIncrementResponse)) netcontext.Context {
	return aetesting.FakeSingleContext(t, "memcache", "Increment", func(req *pb.MemcacheIncrementRequest, res *pb.MemcacheIncrementResponse) error {
		f(req, res)
		return nil
	})
}

// Instead of the real App Engine context, we use a fake one that
// handles a single set request using the callback f().
func fakeSetContext(t *testing.T, f func(req *pb.MemcacheSetRequest, res *pb.MemcacheSetResponse)) netcontext.Context {
	return aetesting.FakeSingleContext(t, "memcache", "Set", func(req *pb.MemcacheSetRequest, res *pb.MemcacheSetResponse) error {
		f(req, res)
		return nil
	})
}

var setNeverCalled = fakeSetContext(nil, func(_ *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
	log.Fatal("service unexpectedly called")
})

// Instead of the real App Engine context, we use a fake one that
// handles a single stats request using the callback f().
func fakeStatsContext(t *testing.T, f func(req *pb.MemcacheStatsRequest, res *pb.MemcacheStatsResponse)) netcontext.Context {
	return aetesting.FakeSingleContext(t, "memcache", "Stats", func(req *pb.MemcacheStatsRequest, res *pb.MemcacheStatsResponse) error {
		f(req, res)
		return nil
	})
}

var statsNeverCalled = fakeStatsContext(nil, func(_ *pb.MemcacheStatsRequest, _ *pb.MemcacheStatsResponse) {
	log.Fatal("service unexpectedly called")
})

func fakeFlushContext(t *testing.T, f func(req *pb.MemcacheFlushRequest, res *pb.MemcacheFlushResponse)) netcontext.Context {
	return aetesting.FakeSingleContext(t, "memcache", "FlushAll", func(req *pb.MemcacheFlushRequest, res *pb.MemcacheFlushResponse) error {
		f(req, res)
		return nil
	})
}

var setStored = fakeSetContext(nil, func(_ *pb.MemcacheSetRequest, res *pb.MemcacheSetResponse) {
	res.SetStatus = append(res.SetStatus, pb.MemcacheSetResponse_STORED)
})

func TestAll(t *testing.T) {
	serviceCalled := false
	tests := []struct {
		desc    string
		ctx     netcontext.Context
		command string
		wants   []string
	}{
		{"get request",
			fakeGetContext(t, func(req *pb.MemcacheGetRequest, _ *pb.MemcacheGetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheGetRequest{
					NameSpace: proto.String(""),
					Key: [][]byte{
						[]byte("aKey"),
					},
					ForCas: proto.Bool(false),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"get aKey\r\n",
			[]string{},
		},
		{"gets request",
			fakeGetContext(t, func(req *pb.MemcacheGetRequest, _ *pb.MemcacheGetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheGetRequest{
					NameSpace: proto.String(""),
					Key: [][]byte{
						[]byte("aKey"),
					},
					ForCas: proto.Bool(true),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"gets aKey\r\n",
			[]string{},
		},
		{"multi get request",
			fakeGetContext(t, func(req *pb.MemcacheGetRequest, _ *pb.MemcacheGetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheGetRequest{
					NameSpace: proto.String(""),
					Key: [][]byte{
						[]byte("key0"),
						[]byte("key1"),
						[]byte("key2"),
					},
					ForCas: proto.Bool(false),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"get key0 key1 key2\r\n",
			[]string{},
		},
		{"get response",
			fakeGetContext(t, func(_ *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse) {
				res.Item = append(res.Item, &pb.MemcacheGetResponse_Item{
					Key:   []byte("aKey"),
					Value: []byte("some value"),
					Flags: proto.Uint32(111),
				})
			}),
			"get aKey\r\n",
			[]string{
				"VALUE aKey 111 10\r\n",
				"some value\r\n",
				"END\r\n",
				timeoutMarker,
			},
		},
		{"gets response with cas",
			fakeGetContext(t, func(_ *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse) {
				res.Item = append(res.Item, &pb.MemcacheGetResponse_Item{
					Key:   []byte("aKey"),
					Value: []byte("some value"),
					Flags: proto.Uint32(111),
					CasId: proto.Uint64(31415),
				})
			}),
			"gets aKey\r\n",
			[]string{
				"VALUE aKey 111 10 31415\r\n",
				"some value\r\n",
				"END\r\n",
				timeoutMarker,
			},
		},
		{"get miss",
			fakeGetContext(t, func(_ *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse) {}),
			"get aKey\r\n",
			[]string{
				"END\r\n",
				timeoutMarker,
			},
		},
		{"multi get response",
			fakeGetContext(t, func(_ *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse) {
				res.Item = append(res.Item, &pb.MemcacheGetResponse_Item{
					Key:   []byte("key1"),
					Value: []byte("some value"),
					Flags: proto.Uint32(111),
				})
				res.Item = append(res.Item, &pb.MemcacheGetResponse_Item{
					Key:   []byte("key2"),
					Value: []byte("another value"),
					Flags: proto.Uint32(222),
				})
			}),
			"get key1 key2\r\n",
			[]string{
				"VALUE key1 111 10\r\n",
				"some value\r\n",
				"VALUE key2 222 13\r\n",
				"another value\r\n",
				"END\r\n",
				timeoutMarker,
			},
		},
		{"get empty item",
			fakeGetContext(t, func(_ *pb.MemcacheGetRequest, res *pb.MemcacheGetResponse) {
				// Return empty item in response.
				res.Item = append(res.Item, &pb.MemcacheGetResponse_Item{})
			}),
			"get aKey\r\n",
			[]string{
				"SERVER_ERROR \"no key returned from server\"\r\n",
				timeoutMarker,
			},
		},
		{"get no keys",
			getNeverCalled,
			"get\r\n", // missing keys
			[]string{
				"ERROR\r\n",
				timeoutMarker,
			},
		},
		{"get server error",
			aetesting.FakeSingleContext(t, "memcache", "Get", func(_ *pb.MemcacheGetRequest, _ *pb.MemcacheGetResponse) error {
				return fmt.Errorf("some server error")
			}),
			"get aKey\r\n",
			[]string{
				"SERVER_ERROR \"some server error\"\r\n",
				timeoutMarker,
			},
		},
		{"delete response",
			fakeDeleteContext(t, func(_ *pb.MemcacheDeleteRequest, res *pb.MemcacheDeleteResponse) {
				res.DeleteStatus = append(res.DeleteStatus, pb.MemcacheDeleteResponse_DELETED)
			}),
			"delete aKey\r\n",
			[]string{
				"DELETED\r\n",
				timeoutMarker,
			},
		},
		{"delete no key",
			getNeverCalled,
			"delete\r\n", // missing keys
			[]string{
				"ERROR\r\n",
				timeoutMarker,
			},
		},
		{"delete no key noreply",
			getNeverCalled,
			"delete noreply\r\n", // missing keys
			[]string{
				"ERROR\r\n",
				timeoutMarker,
			},
		},
		{"increment response",
			fakeIncrContext(t, func(_ *pb.MemcacheIncrementRequest, res *pb.MemcacheIncrementResponse) {
				res.IncrementStatus = pb.MemcacheIncrementResponse_OK.Enum()
				res.NewValue = proto.Uint64(315)
			}),
			"incr aKey 1\r\n",
			[]string{
				"315\r\n",
				timeoutMarker,
			},
		},
		{"increment response noreply",
			fakeIncrContext(t, func(_ *pb.MemcacheIncrementRequest, res *pb.MemcacheIncrementResponse) {
				res.IncrementStatus = pb.MemcacheIncrementResponse_OK.Enum()
				res.NewValue = proto.Uint64(315)
			}),
			"incr aKey 1 noreply\r\n",
			[]string{
				timeoutMarker,
			},
		},
		{"increment response error",
			fakeIncrContext(t, func(_ *pb.MemcacheIncrementRequest, res *pb.MemcacheIncrementResponse) {
				res.IncrementStatus = pb.MemcacheIncrementResponse_ERROR.Enum()
			}),
			"incr aKey 1\r\n",
			[]string{
				"ERROR\r\n",
				timeoutMarker,
			},
		},
		{"set request",
			fakeSetContext(t, func(req *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheSetRequest{
					NameSpace: proto.String(""),
					Item: []*pb.MemcacheSetRequest_Item{
						&pb.MemcacheSetRequest_Item{
							Key:            []byte("aKey"),
							Value:          []byte("some value"),
							Flags:          proto.Uint32(111),
							SetPolicy:      pb.MemcacheSetRequest_SET.Enum(),
							ExpirationTime: proto.Uint32(3600),
						},
					},
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"set aKey 111 3600 10\r\nsome value\r\n",
			[]string{},
		},
		{"set request max memcache size ",
			fakeSetContext(t, func(req *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheSetRequest{
					NameSpace: proto.String(""),
					Item: []*pb.MemcacheSetRequest_Item{
						&pb.MemcacheSetRequest_Item{
							Key:            []byte("aMaxPayloadKey"),
							Value:          []byte(maxMemcachePayloadValue),
							Flags:          proto.Uint32(111),
							SetPolicy:      pb.MemcacheSetRequest_SET.Enum(),
							ExpirationTime: proto.Uint32(3600),
						},
					},
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"set aMaxPayloadKey 111 3600 " + strconv.Itoa(len(maxMemcachePayloadValue)) + "\r\n" + maxMemcachePayloadValue + "\r\n",
			[]string{},
		},
		{"set request with whitespace",
			fakeSetContext(t, func(req *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheSetRequest{
					NameSpace: proto.String(""),
					Item: []*pb.MemcacheSetRequest_Item{
						&pb.MemcacheSetRequest_Item{
							Key:            []byte("anotherKey"),
							Value:          []byte("ANOTHER VALUE"),
							Flags:          proto.Uint32(22222),
							SetPolicy:      pb.MemcacheSetRequest_SET.Enum(),
							ExpirationTime: proto.Uint32(7200),
						},
					},
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			// Make sure we can handle extra tabs and blanks.
			"   set   anotherKey\t22222\t\t7200    13   \r\nANOTHER VALUE\r\n",
			[]string{},
		},
		{"set response",
			setStored,
			"set aKey 111 3600 10\r\nsome value\r\n",
			[]string{
				"STORED\r\n",
				timeoutMarker,
			},
		},
		{"set response no reply",
			setStored,
			"set aKey 111 3600 10 noreply\r\nsome value\r\n",
			[]string{
				timeoutMarker,
			},
		},
		{"set response bogus no reply",
			setStored,
			// Instead of "noreply" add bogus token
			"set aKey 111 3600 10 aBogusToken\r\nsome value\r\n",
			[]string{
				"STORED\r\n",
				timeoutMarker,
			},
		},
		{"set wrong number of tokens",
			setNeverCalled,
			"set foo 0 0\r\n", // missing bytes token
			[]string{
				"ERROR\r\n",
				timeoutMarker,
			},
		},
		{"set bad flags",
			setNeverCalled,
			"set foo xxxx 0 5\r\n", // non-integer flags
			[]string{
				"CLIENT_ERROR bad command line format\r\n",
				timeoutMarker,
			},
		},
		{"set bad expiry",
			setNeverCalled,
			"set foo 0 xxxx 5\r\n", // non-integer expiry
			[]string{
				"CLIENT_ERROR bad command line format\r\n",
				timeoutMarker,
			},
		},
		{"set bad termination",
			setNeverCalled,
			"set aKey 111 3600 10\r\nsome valueXX", // XX instead of \r\n
			[]string{
				"CLIENT_ERROR bad data chunk\r\n",
				timeoutMarker,
			},
		},
		{"set empty response",
			fakeSetContext(t, func(_ *pb.MemcacheSetRequest, res *pb.MemcacheSetResponse) {}),
			"set aKey 111 3600 10\r\nsome value\r\n",
			[]string{
				"SERVER_ERROR \"server did not return a status\"\r\n",
				timeoutMarker,
			},
		},
		{"set server error",
			aetesting.FakeSingleContext(t, "memcache", "Set", func(_ *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) error {
				return fmt.Errorf("some server error")
			}),
			"set aKey 111 3600 10\r\nsome value\r\n",
			[]string{
				"SERVER_ERROR \"some server error\"\r\n",
				timeoutMarker,
			},
		},
		{"add request",
			fakeSetContext(t, func(req *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheSetRequest{
					NameSpace: proto.String(""),
					Item: []*pb.MemcacheSetRequest_Item{
						&pb.MemcacheSetRequest_Item{
							Key:            []byte("aKey"),
							Value:          []byte("some value"),
							Flags:          proto.Uint32(111),
							SetPolicy:      pb.MemcacheSetRequest_ADD.Enum(),
							ExpirationTime: proto.Uint32(3600),
						},
					},
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"add aKey 111 3600 10\r\nsome value\r\n",
			[]string{},
		},
		{"replace request",
			fakeSetContext(t, func(req *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheSetRequest{
					NameSpace: proto.String(""),
					Item: []*pb.MemcacheSetRequest_Item{
						&pb.MemcacheSetRequest_Item{
							Key:            []byte("aKey"),
							Value:          []byte("some value"),
							Flags:          proto.Uint32(111),
							SetPolicy:      pb.MemcacheSetRequest_REPLACE.Enum(),
							ExpirationTime: proto.Uint32(3600),
						},
					},
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"replace aKey 111 3600 10\r\nsome value\r\n",
			[]string{},
		},
		{"cas request",
			fakeSetContext(t, func(req *pb.MemcacheSetRequest, _ *pb.MemcacheSetResponse) {
				serviceCalled = true
				// Test request.
				want := &pb.MemcacheSetRequest{
					NameSpace: proto.String(""),
					Item: []*pb.MemcacheSetRequest_Item{
						&pb.MemcacheSetRequest_Item{
							Key:            []byte("aKey"),
							Value:          []byte("some value"),
							Flags:          proto.Uint32(111),
							SetPolicy:      pb.MemcacheSetRequest_CAS.Enum(),
							CasId:          proto.Uint64(31415926535),
							ExpirationTime: proto.Uint32(3600),
							ForCas:         proto.Bool(true),
						},
					},
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"cas aKey 111 3600 10 31415926535\r\nsome value\r\n",
			[]string{},
		},
		{"incr request",
			fakeIncrContext(t, func(req *pb.MemcacheIncrementRequest, _ *pb.MemcacheIncrementResponse) {
				serviceCalled = true
				want := &pb.MemcacheIncrementRequest{
					Key:       []byte("aKey"),
					Delta:     proto.Uint64(314),
					NameSpace: proto.String(""),
					Direction: pb.MemcacheIncrementRequest_INCREMENT.Enum(),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"incr aKey 314\r\n",
			[]string{},
		},
		{"incr request no arg",
			fakeIncrContext(t, func(req *pb.MemcacheIncrementRequest, _ *pb.MemcacheIncrementResponse) {
				serviceCalled = true
				want := &pb.MemcacheIncrementRequest{
					Key:       []byte("aKey"),
					Delta:     proto.Uint64(1),
					NameSpace: proto.String(""),
					Direction: pb.MemcacheIncrementRequest_INCREMENT.Enum(),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"incr aKey\r\n",
			[]string{},
		},
		{"decr request",
			fakeIncrContext(t, func(req *pb.MemcacheIncrementRequest, _ *pb.MemcacheIncrementResponse) {
				serviceCalled = true
				want := &pb.MemcacheIncrementRequest{
					Key:       []byte("aKey"),
					Delta:     proto.Uint64(314),
					NameSpace: proto.String(""),
					Direction: pb.MemcacheIncrementRequest_DECREMENT.Enum(),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"decr aKey 314\r\n",
			[]string{},
		},
		{"decr request no arg",
			fakeIncrContext(t, func(req *pb.MemcacheIncrementRequest, _ *pb.MemcacheIncrementResponse) {
				serviceCalled = true
				want := &pb.MemcacheIncrementRequest{
					Key:       []byte("aKey"),
					Delta:     proto.Uint64(1),
					NameSpace: proto.String(""),
					Direction: pb.MemcacheIncrementRequest_DECREMENT.Enum(),
				}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"decr aKey\r\n",
			[]string{},
		},
		{"flush request with ignored ts",
			fakeFlushContext(t, func(req *pb.MemcacheFlushRequest, _ *pb.MemcacheFlushResponse) {
				serviceCalled = true
				want := &pb.MemcacheFlushRequest{}
				if !proto.Equal(req, want) {
					t.Errorf("got  <%s>\nwant <%s>",
						proto.MarshalTextString(req),
						proto.MarshalTextString(want))
				}
			}),
			"flush_all 123 noreply\r\n",
			[]string{},
		},
		{"flush response always OK",
			fakeFlushContext(t, func(req *pb.MemcacheFlushRequest, _ *pb.MemcacheFlushResponse) {
			}),
			"flush_all 123\r\n",
			[]string{"OK\r\n",
				timeoutMarker,
			},
		},
		{"flush no argument",
			fakeFlushContext(t, func(req *pb.MemcacheFlushRequest, _ *pb.MemcacheFlushResponse) {
			}),
			"flush_all\r\n",
			[]string{"OK\r\n",
				timeoutMarker,
			},
		},
		{"stats request",
			fakeStatsContext(t, func(req *pb.MemcacheStatsRequest, _ *pb.MemcacheStatsResponse) {
				serviceCalled = true
			}),
			"stats\r\n",
			[]string{},
		},
		{"stats response",
			fakeStatsContext(t, func(_ *pb.MemcacheStatsRequest, res *pb.MemcacheStatsResponse) {
				res.Stats = &pb.MergedNamespaceStats{
					Hits:          proto.Uint64(111),
					Misses:        proto.Uint64(222),
					ByteHits:      proto.Uint64(333),
					Items:         proto.Uint64(444),
					Bytes:         proto.Uint64(555),
					OldestItemAge: proto.Uint32(666),
				}
			}),
			"stats\r\n",
			[]string{
				"STAT get_hits 111\r\n",
				"STAT get_misses 222\r\n",
				"STAT bytes_read 333\r\n",
				"STAT curr_items 444\r\n",
				"STAT bytes 555\r\n",
				"STAT oldest_item_age 666\r\n",
				"END\r\n",
				timeoutMarker,
			},
		},
		{"stats missing",
			// TODO(eobrain) Check that it really is OK
			// and expected for the memcache_service to
			// have missing stats, which is the case after
			// a flush.
			fakeStatsContext(t, func(_ *pb.MemcacheStatsRequest, res *pb.MemcacheStatsResponse) {}),
			"stats\r\n",
			[]string{
				"END\r\n",
				timeoutMarker,
			},
		},
		{"stats server error",
			aetesting.FakeSingleContext(t, "memcache", "Stats", func(_ *pb.MemcacheStatsRequest, _ *pb.MemcacheStatsResponse) error {
				return fmt.Errorf("some server error")
			}),
			"stats\r\n",
			[]string{
				"SERVER_ERROR \"some server error\"\r\n",
				timeoutMarker,
			},
		},
		{"empty line",
			statsNeverCalled,
			"\r\n", // Send a blank line
			[]string{
				"ERROR\r\n",
				timeoutMarker,
			},
		},
		{"version request",
			nil,
			"version\r\n",
			[]string{
				"VERSION not implemented\r\n",
				timeoutMarker,
			},
		},
	}

	for i, test := range tests {
		serviceCalled = false
		log.Printf("----- Begin: %s", test.desc)
		gots := sendCommand(test.ctx, test.command, len(test.wants))
		for i, want := range test.wants {
			if gots[i] != want {
				t.Errorf("test %d, %s: got %q want %q", i, test.desc, gots[i], want)
			}
		}
		// If not expecting any output, make sure
		// serviceCalled flag was set by context.
		if len(test.wants) == 0 && !serviceCalled {
			t.Errorf("test %d, %s: service was not called as expected", i, test.desc)
		}
		log.Printf("----- End: %s", test.desc)
	}
}
