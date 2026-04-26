package main

import (
	"fmt"
	"log"
	"os"
)

var (
	version = "0.1.0"
	commit  = "dev"
	builtAt = "1970-01-01T00:00:00Z"
)

func main() {
	if len(os.Args) != 2 {
		usage()
		os.Exit(1)
	}

	switch os.Args[1] {
	case "up", "down", "status":
		log.Printf("migrate placeholder: %s (implemented in PR-2)", os.Args[1])
	default:
		usage()
		os.Exit(1)
	}
}

func usage() {
	fmt.Fprintln(os.Stderr, "usage: migrate [up|down|status]")
}
