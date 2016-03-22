package main

import (
	"bytes"
	"io/ioutil"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestMux(t *testing.T) {
	handler := makeHandler()
	server := httptest.NewServer(handler)
	defer server.Close()

	for _, path := range []string{"/", "/path"} {
		resp, err := http.Get(server.URL + path)
		if err != nil {
			t.Fatal(err)
		}
		if resp.StatusCode != 400 {
			t.Fatalf("Received non-400 response: %d\n", resp.StatusCode)
		}
		defer resp.Body.Close()
		body, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			t.Fatal(err)
		}
		if !bytes.Contains(body, []byte("App Engine APIs over the Service Bridge are disabled")) {
			t.Fatalf("Expected disabled message. Got '%s'\n", body)
		}
	}
}
