#!/bin/bash
set -e

function run {
	dd if=/dev/urandom of=tmp bs=50K count=1
	curl -F "file=@tmp;filename=1.txt" http://127.0.0.1:5000/$1/a/b
	echo
	wget --post-file=./tmp 127.0.0.1:5000/$1/1/2/3/2.txt
	wget http://127.0.0.1:5000/$1/a/b/1.txt
	wget http://127.0.0.1:5000/$1/1/2/3/2.txt
	wget --spider http://127.0.0.1:5000/$1/a/b/1.txt
	wget --spider http://127.0.0.1:5000/$1/1/2/3/2.txt
	curl -X DELETE http://127.0.0.1:5000/$1/1
	curl -X DELETE http://127.0.0.1:5000/$1/a
}

run dbg
run cfg
run pro

