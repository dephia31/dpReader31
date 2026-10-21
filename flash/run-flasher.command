#!/bin/zsh
cd "${0:A:h}" || exit 1
python3 -m http.server 8765 --bind 127.0.0.1 &
server_pid=$!
sleep 1
open -a "Google Chrome" "http://127.0.0.1:8765/"
wait "$server_pid"
