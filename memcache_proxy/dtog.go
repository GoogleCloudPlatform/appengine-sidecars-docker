// Package dtog is a proxy that provides a memcached network protocol
// connection to the App Engine memcache_service.
package dtog

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"log"
	"net"
	"strconv"

	"github.com/golang/protobuf/proto"
	gaeint "google.golang.org/appengine/notreallyinternal"
	pb "google.golang.org/appengine/notreallyinternal/memcache"
	netcontext "golang.org/x/net/context"
)

// Proxy is the proxy server running on some host:port Address.
type Proxy struct {
	BindingAddr string
	ln          net.Listener
	close       chan chan error
}

func new(bindingAddr string) (*Proxy, error) {
	px := &Proxy{
		BindingAddr: bindingAddr,
		close:       make(chan chan error, 1),
	}

	var err error
	if px.ln, err = net.Listen("tcp", px.BindingAddr); err != nil {
		return nil, err
	}
	return px, nil
}

// StartAsync starts a new proxy bridge server in the background.
// The provided context must remain valid until the Close method is called.
//
// TODO(eobrain) Remove async call and only have blocking
// call. (Caller can always use "go" to make call async.) See
// http://go/channelsandcallbacks
func StartAsync(ctx netcontext.Context, bindingAddr string) (*Proxy, error) {
	px, err := new(bindingAddr)
	if err != nil {
		return nil, err
	}
	go px.listenLoop(ctx)
	return px, nil
}

// StartSync starts a new proxy bridge server and does not return
// except in the case of an error.
func StartSync(ctx netcontext.Context, bindingAddr string) error {
	px, err := new(bindingAddr)
	if err != nil {
		return err
	}
	px.listenLoop(ctx) // should never return
	return fmt.Errorf("proxy listen loop exited unexpectedly")
}

// Close shuts down the listener.
func (p *Proxy) Close() error {
	// Note using channel to signal a close action rather than
	// using the usual google3 pattern of a Context because it is
	// important to avoid google3 dependencies on this code what
	// must run in a managed VM environment.
	response := make(chan error, 1)
	p.close <- response
	return <-response
}

func (p *Proxy) listenLoop(ctx netcontext.Context) {
	newAcceptCh := make(chan bool, 1)
	newAcceptCh <- true
	for {
		select {
		case ch := <-p.close: // ch sends back the error
			ch <- p.ln.Close()
			return

		case <-newAcceptCh:
			go func() {
				conn, err := p.ln.Accept()
				newAcceptCh <- true
				if err != nil {
					log.Printf("WARNING: Listener.Accept: %v", err)
					return
				}
				serveConn(conn, ctx)
			}()
		}
	}
}

// This is a simple helper to inserts the \r\n terminator
type lineWriter struct {
	bufio.Writer
}

func (out *lineWriter) printfLn(format string, args ...interface{}) {
	fmt.Fprintf(out, format, args...)
	out.WriteString("\r\n") // memcached protocol requires that as a line terminator.
}

func (out *lineWriter) printLn(args ...[]byte) {
	for _, arg := range args {
		out.Write(arg)
		out.WriteString("\r\n") // memcached protocol requires that as a line terminator.
	}
}

// stream encapsulates the communication up the stack to the socket
// server and down the stack to the App Engine context.
type streams struct {
	ctx netcontext.Context
	in  *bufio.Reader
	out *lineWriter
}

// memcacheError and its three concrete implementation provide an
// error handling mechanism for echoing errors back to the memcached
// protocol socket and also logging the errors.
type memcacheError interface {
	Error() string
	writeTo(*streams)
}

// Implements memcacheError.
type badCommand string

func badCommandf(format string, args ...interface{}) badCommand {
	return badCommand(fmt.Sprintf(format, args...))
}

func (e badCommand) Error() string {
	return fmt.Sprintf("bad command error %s", string(e))
}

func (e badCommand) writeTo(s *streams) {
	s.out.printfLn("ERROR")
	log.Print(e.Error())
}

// Implements memcacheError.
type clientError struct{ echo, log string }

func (e clientError) Error() string {
	return fmt.Sprintf("client error %s -- %s", e.echo, e.log)
}
func (e clientError) writeTo(s *streams) {
	s.out.printfLn("CLIENT_ERROR %s", e.echo)
	log.Print(e.Error())
}

// Implements memcacheError.
type serverError struct{ cause error }

func (e serverError) Error() string {
	return e.cause.Error()
}
func (e serverError) writeTo(s *streams) {
	s.out.printfLn("SERVER_ERROR %q", e.cause)
	log.Print(e.Error())
}

// This represents the various flavors of store commands.
type storePolicy int

const (
	set storePolicy = iota
	add
	replace
)

// serveConn is the main loop serving one connection to the proxy.
func serveConn(conn net.Conn, ctx netcontext.Context) {
	defer conn.Close()
	s := &streams{ctx, bufio.NewReader(conn), &lineWriter{*bufio.NewWriter(conn)}}
	for {
		line, err := s.in.ReadBytes('\n')
		if err != nil {
			if err != io.EOF {
				log.Printf("ERROR: Client read error: %v", err)
			}
			return
		}
		// Note, to avoid unnecessary allocation that might
		// cause GC pauses we avoid converting to strings
		// except when necessary.
		if args := bytes.Fields(bytes.TrimSpace(line)); len(args) == 0 {
			badCommandf("bogus empty line").writeTo(s)
		} else {
			if err := s.demux(string(args[0]), args[1:]...); err != nil {
				err, ok := err.(memcacheError)
				if !ok { // We don't actually expect this to happen, but let's be paranoid.
					err = serverError{fmt.Errorf("internal problem: %v", err)}
				}
				err.writeTo(s)
			}
		}
		s.out.Flush()
	}
}

func (s *streams) demux(command string, args ...[]byte) error {
	// TODO(eobrain) add all the other protocol commands to the switch.
	switch command {
	case "get":
		return s.get(false, args...)
	case "gets":
		return s.get(true, args...)
	case "set":
		return s.store(set, args...)
	case "add":
		return s.store(add, args...)
	case "replace":
		return s.store(replace, args...)
	case "cas":
		return s.cas(args...)
	case "delete":
		return s.delete(args...)
	case "incr":
		return s.incr(false, args...)
	case "decr":
		return s.incr(true, args...)
	case "flush_all":
		return s.flush(args...)
	case "stats":
		return s.stats()
	default:
		return serverError{fmt.Errorf("unimplemented command")}
	}
}

func (s *streams) call(method string, req, res proto.Message) *serverError {
	if err := gaeint.Call(s.ctx, "memcache", method, req, res); err != nil {
		return &serverError{err}
	}
	return nil
}

// get handles the "get" and "gets" commands on the memcached socket and invokes
// the corresponding memcache_service stubby call.
func (s *streams) get(cas bool, args ...[]byte) error {
	if len(args) == 0 {
		return badCommandf("no keys given to get command")
	}

	req := &pb.MemcacheGetRequest{
		NameSpace: proto.String(""),
		Key:       args,
		ForCas:    &cas,
	}

	res := &pb.MemcacheGetResponse{}
	if err := s.call("Get", req, res); err != nil {
		return err
	}

	for _, item := range res.Item {
		key := string(item.Key)
		if key == "" {
			return serverError{fmt.Errorf("no key returned from server")}
		}
		if cas && item.CasId == nil {
			return serverError{fmt.Errorf("no casId returned from a cas request")}
		}

		flags := item.GetFlags()
		length := len(item.Value)
		if cas {
			s.out.printfLn("VALUE %s %d %d %d", key, flags, length, item.GetCasId())
		} else {
			s.out.printfLn("VALUE %s %d %d", key, flags, length)
		}
		s.out.printLn(item.Value)
	}
	s.out.printLn([]byte("END"))
	return nil
}

// delete handles the "delete" command on the memcached socket and invokes
// the corresponding memcache_service stubby call.
func (s *streams) delete(args ...[]byte) error {
	length := len(args)

	if length == 0 ||
		length == 1 && string(args[0]) == "noreply" ||
		length == 2 && string(args[1]) != "noreply" {
		return badCommandf("no key given to delete command")
	}

	if length > 2 || length == 2 && string(args[1]) != "noreply" {
		return badCommandf("only one key is supported for the delete command")
	}
	noreply := length == 2

	items := []*pb.MemcacheDeleteRequest_Item{
		&pb.MemcacheDeleteRequest_Item{Key: args[0]},
	}

	req := &pb.MemcacheDeleteRequest{
		NameSpace: proto.String(""),
		Item:      items,
	}

	res := &pb.MemcacheDeleteResponse{}

	// TODO(gbin) make this async in case of noreply
	if err := s.call("Delete", req, res); err != nil {
		return err
	}

	if noreply {
		return nil
	}

	if res.DeleteStatus[0] == pb.MemcacheDeleteResponse_DELETED {
		s.out.printLn([]byte("DELETED"))
	} else {
		s.out.printLn([]byte("NOT_FOUND"))
	}
	return nil
}

func unpackFirstStorageCmdArgs(args ...[]byte) (key []byte, flags uint32,
	exptime uint32, bytes uint32, err error) {

	key = args[0]

	flagsUpc, err := strconv.ParseUint(string(args[1]), 10, 32)
	if err != nil {
		err = clientError{"bad command line format",
			fmt.Sprintf("<flags> should be an unsigned 32 bits integer but got %s [%s].",
				string(args[1]), err)}
		return
	}
	flags = uint32(flagsUpc)

	exptimeUpc, err := strconv.ParseUint(string(args[2]), 10, 32)
	if err != nil {
		err = clientError{"bad command line format",
			fmt.Sprintf("<exptime> should be an unsigned 32 bits integer but got %s [%s].",
				string(args[2]), err)}
		return
	}
	exptime = uint32(exptimeUpc)

	bytesUpc, err := strconv.ParseUint(string(args[3]), 10, 32)
	if err != nil {
		err = clientError{"bad command line format",
			fmt.Sprintf("<bytes> should be an unsigned 32 bits integer but got %s [%s].",
				string(args[3]), err)}
		return
	}
	bytes = uint32(bytesUpc)
	return
}

func unpackStorageCmdArgs(args ...[]byte) (key []byte, flags uint32,
	exptime uint32, bytes uint32, noreply bool, err error) {
	if n := len(args); n != 4 && n != 5 {
		err = badCommandf("got %d Set arguments instead of 4 or 5.", n)
		return
	}

	key, flags, exptime, bytes, err = unpackFirstStorageCmdArgs(args...)
	if err != nil {
		return
	}

	noreply = len(args) == 5 && string(args[4]) == "noreply"
	return
}

func unpackCASCmdArgs(args ...[]byte) (key []byte, flags uint32, exptime uint32,
	bytes uint32, casUnique uint64, noreply bool, err error) {
	if n := len(args); n != 5 && n != 6 {
		err = badCommandf("got %d Set arguments instead of 5 or 6.", n)
		return
	}

	key, flags, exptime, bytes, err = unpackFirstStorageCmdArgs(args...)
	if err != nil {
		return
	}

	casUnique, err = strconv.ParseUint(string(args[4]), 10, 64)
	if err != nil {
		err = clientError{"bad command line format",
			fmt.Sprintf("<cas_unique> should be a unsigned 64 bits integer but got %s [%s].",
				string(args[4]), err)}
		return
	}

	noreply = len(args) == 6 && string(args[5]) == "noreply"
	return
}

func (s *streams) incr(decrement bool, args ...[]byte) error {
	if n := len(args); n == 0 || n > 3 {
		return badCommandf("got %d Increment arguments instead of 1, 2 or 3.", n)
	}
	var (
		noreply bool
		err     error
	)
	value := uint64(1) // by default incr by 1
	if len(args) > 1 {
		if len(args) == 2 && string(args[1]) == "noreply" {
			noreply = true
		} else {
			value, err = strconv.ParseUint(string(args[1]), 10, 64)
			if err != nil {
				return clientError{"bad command line format",
					fmt.Sprintf("<value> should be an unsigned 64 bits integer but got %s [%s].",
						string(args[1]), err)}
			}
			noreply = len(args) == 3 && string(args[2]) == "noreply"
		}
	}

	var direction pb.MemcacheIncrementRequest_Direction
	if decrement {
		direction = pb.MemcacheIncrementRequest_DECREMENT
	} else {
		direction = pb.MemcacheIncrementRequest_INCREMENT
	}

	// Call memcache_service.
	req := &pb.MemcacheIncrementRequest{
		Key:       args[0],
		Direction: direction.Enum(),
		NameSpace: proto.String(""),
		Delta:     &value,
	}
	res := &pb.MemcacheIncrementResponse{}
	if err := s.call("Increment", req, res); err != nil {
		ie, ok := err.cause.(*gaeint.APIError)
		if !ok {
			return err
		}
		if ie.Code == 6 {
			s.out.printfLn("CLIENT_ERROR cannot increment or decrement non-numeric value") // Copy pasted from the real memcached.
			return nil
		}
		if ie.Code == 1 { // This actually means that the value is non existent so don't SERVER_ERROR on it.
			s.out.printfLn("NOT_FOUND")
			return nil
		}
		return err
	}

	if !noreply {
		if res.IncrementStatus != nil && *res.IncrementStatus == pb.MemcacheIncrementResponse_ERROR {
			s.out.printfLn("ERROR")
			return nil
		}
		if res.NewValue != nil {
			s.out.printfLn("%d", *res.NewValue)
		}
	}
	return nil
}

func (s *streams) readValue(expectedLen uint32) ([]byte, error) {
	terminatedLen := expectedLen + 2 // + 2 for \r\n
	value := make([]byte, terminatedLen)
	n, err := s.in.Read(value)
	if err != nil {
		return nil, clientError{"bad command line format",
			fmt.Sprintf("reading value %v", err),
		}
	}
	if uint32(n) != terminatedLen {
		return nil, clientError{"bad command line format",
			fmt.Sprintf("could only read %d of %d bytes", n, terminatedLen),
		}
	}

	if value[terminatedLen-2] != '\r' || value[terminatedLen-1] != '\n' {
		return nil, clientError{"bad data chunk",
			fmt.Sprintf(`got %q instead of expected "\r\n" terminator`, value[terminatedLen-2:]),
		}
	}
	return value[:terminatedLen-2], nil
}

// store handles the "set", "add" and "replace" commands on the memcached socket and invokes
// the corresponding memcache_service stubby call.
func (s *streams) store(policy storePolicy, args ...[]byte) error {
	key, flags, exptime, bytes, noreply, err := unpackStorageCmdArgs(args...)
	if err != nil {
		return err
	}

	var value []byte
	if value, err = s.readValue(bytes); err != nil {
		return err
	}

	var reqPolicy pb.MemcacheSetRequest_SetPolicy
	switch policy {
	case set:
		reqPolicy = pb.MemcacheSetRequest_SET
	case add:
		reqPolicy = pb.MemcacheSetRequest_ADD
	case replace:
		reqPolicy = pb.MemcacheSetRequest_REPLACE
	}

	// Call memcache_service.
	req := &pb.MemcacheSetRequest{
		NameSpace: proto.String(""),
		Item: []*pb.MemcacheSetRequest_Item{
			&pb.MemcacheSetRequest_Item{
				Key:            key,
				Value:          value,
				Flags:          &flags,
				SetPolicy:      &reqPolicy,
				ExpirationTime: &exptime,
			},
		},
	}
	res := &pb.MemcacheSetResponse{}
	if err := s.call("Set", req, res); err != nil {
		return err
	}
	if n := len(res.SetStatus); n == 0 {
		return serverError{fmt.Errorf("server did not return a status")}
	} else if n > 1 {
		return serverError{fmt.Errorf("got %d statuses instead of a single status from server", n)}
	}

	// Write the status response.
	if !noreply {
		s.out.printfLn(res.SetStatus[0].String())
	}
	return nil
}

// cas handles the "cas" command on the memcached socket and invokes
// the corresponding memcache_service stubby call.
func (s *streams) cas(args ...[]byte) error {
	key, flags, exptime, bytes, casUnique, noreply, err := unpackCASCmdArgs(args...)

	if err != nil {
		return err
	}

	var value []byte
	if value, err = s.readValue(bytes); err != nil {
		return err
	}

	// Call memcache_service.
	req := &pb.MemcacheSetRequest{
		NameSpace: proto.String(""),
		Item: []*pb.MemcacheSetRequest_Item{
			&pb.MemcacheSetRequest_Item{
				Key:            key,
				Value:          value,
				Flags:          &flags,
				SetPolicy:      pb.MemcacheSetRequest_CAS.Enum(),
				CasId:          &casUnique,
				ExpirationTime: &exptime,
				ForCas:         proto.Bool(true),
			},
		},
	}
	res := &pb.MemcacheSetResponse{}
	if err := s.call("Set", req, res); err != nil {
		return err
	}
	if n := len(res.SetStatus); n == 0 {
		return serverError{fmt.Errorf("server did not return a status")}
	} else if n > 1 {
		return serverError{fmt.Errorf("got %d statuses instead of a single status from server", n)}
	}

	// Write the status response.
	if !noreply {
		s.out.printfLn(res.SetStatus[0].String())
	}
	return nil
}

// flush handles the "flush_all" command on the memcached socket and invokes
// the corresponding memcache_service stubby call.
func (s *streams) flush(args ...[]byte) error {

	if n := len(args); n > 2 {
		return badCommandf("got %d flush_all arguments instead of 0 to 2.", n)
	}
	var (
		noreply bool
		err     error
	)

	if len(args) > 0 {
		if string(args[0]) == "noreply" {
			noreply = true
		} else {
			_, err := strconv.ParseUint(string(args[0]), 10, 32) // Parse but ignore expiration

			if err != nil {
				return clientError{"bad command line format",
					fmt.Sprintf("<expiration> should be an unsigned 32 bits integer but got %s [%s].",
						string(args[0]), err)}
			}
			noreply = len(args) == 2 && string(args[1]) == "noreply"
		}

		if err != nil {
			return err
		}
	}

	// Call memcache_service.
	req := &pb.MemcacheFlushRequest{}
	res := &pb.MemcacheFlushResponse{}
	if err := s.call("FlushAll", req, res); err != nil {
		return err
	}

	if !noreply {
		s.out.printfLn("OK")
	}
	return nil
}

// stats handles the "stats" command on the memcached socket and
// invokes the corresponding memcache_service stubby call.
func (s *streams) stats() error {
	// TODO(eobrain) parse command arguments.
	// TODO(eobrain) use a sync.Pool of proto message objects, here and elsewhere
	req := &pb.MemcacheStatsRequest{}
	res := &pb.MemcacheStatsResponse{}
	if err := s.call("Stats", req, res); err != nil {
		return err
	}
	if res.Stats != nil {
		s.out.printfLn("STAT get_hits %d", res.Stats.GetHits())
		s.out.printfLn("STAT get_misses %d", res.Stats.GetMisses())
		s.out.printfLn("STAT bytes_read %d", res.Stats.GetByteHits())
		s.out.printfLn("STAT curr_items %d", res.Stats.GetItems())
		s.out.printfLn("STAT bytes %d", res.Stats.GetBytes())
		s.out.printfLn("STAT oldest_item_age %d", res.Stats.GetOldestItemAge())
	}
	s.out.printLn([]byte("END"))
	return nil
}
