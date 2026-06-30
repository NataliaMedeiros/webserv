#!/usr/bin/env bash
set -u

#create variables
PORT="${PORT:-18080}" #port where the server will run
CONF=".test_http_response.conf" #temporary config file
ROOT=".test_www" #temporary www folder
LOG=".test_webserv.log" #file where the server logs are stored
PID="" #server process ID
PASS=0 #number of tests that passed
FAIL=0 #number of tests that failed

#This function kill the server when the test is finished
cleanup() {
    if [ -n "${PID}" ] && kill -0 "${PID}" 2>/dev/null; then
        kill "${PID}" 2>/dev/null || true
        wait "${PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

#This is just to print a colorfull answer
ok() {
    printf "\033[32m[OK]\033[0m %s\n" "$1"
    PASS=$((PASS + 1))
}

#This is just to print a colorfull answer
ko() {
    printf "\033[31m[FAIL]\033[0m %s\n" "$1"
    FAIL=$((FAIL + 1))
}

expect_contains() {
    haystack="$1"
    needle="$2"
    label="$3"
    if printf "%s" "$haystack" | grep -Fqi "$needle"; then
        ok "$label"
    else
        ko "$label"
        printf "Expected to find: %s\n" "$needle"
        printf "Response was:\n%s\n" "$haystack"
    fi
}

expect_not_contains() {
    haystack="$1"
    needle="$2"
    label="$3"
    if printf "%s" "$haystack" | grep -Fqi "$needle"; then
        ko "$label"
        printf "Did not expect to find: %s\n" "$needle"
        printf "Response was:\n%s\n" "$haystack"
    else
        ok "$label"
    fi
}

request() {
    method="$1"
    path="$2"
    shift 2
    if [ "$method" = "POST" ]; then
        # Your parser requires POST to have Content-Length or Transfer-Encoding.
        curl -sS -i --max-time 3 -X "$method" --data-binary "" "$@" "http://127.0.0.1:${PORT}${path}"
    else
        curl -sS -i --max-time 3 -X "$method" "$@" "http://127.0.0.1:${PORT}${path}"
    fi
}

expect_status() {
    method="$1"
    path="$2"
    expected="$3"
    label="$4"
    resp="$(request "$method" "$path")"
    first_line="$(printf "%s" "$resp" | sed -n '1p' | tr -d '\r')"
    case "$first_line" in
        "HTTP/1.1 ${expected}"*) ok "$label" ;;
        *) ko "$label"; printf "Expected HTTP %s, got: %s\nFull response:\n%s\n" "$expected" "$first_line" "$resp" ;;
    esac
}

expect_status_with_header() {
    method="$1"
    path="$2"
    expected="$3"
    header="$4"
    label="$5"
    resp="$(request "$method" "$path")"
    first_line="$(printf "%s" "$resp" | sed -n '1p' | tr -d '\r')"
    if printf "%s" "$first_line" | grep -Fq "HTTP/1.1 ${expected}" && printf "%s" "$resp" | grep -Fqi "$header"; then
        ok "$label"
    else
        ko "$label"
        printf "Expected HTTP %s and header containing: %s\nGot first line: %s\nFull response:\n%s\n" "$expected" "$header" "$first_line" "$resp"
    fi
}

printf "Preparing test files...\n"
rm -rf "$ROOT" "$CONF" "$LOG"
mkdir -p "$ROOT/errors" "$ROOT/files" "$ROOT/list" "$ROOT/noindex"
printf 'ROOT INDEX OK\n' > "$ROOT/index.html"
printf 'hello response\n' > "$ROOT/files/hello.txt"
printf 'delete me\n' > "$ROOT/files/delete-me.txt"
printf 'CUSTOM 404 PAGE\n' > "$ROOT/errors/404.html"
printf 'CUSTOM 403 PAGE\n' > "$ROOT/errors/403.html"
printf 'CUSTOM 405 PAGE\n' > "$ROOT/errors/405.html"
printf 'autoindex A\n' > "$ROOT/list/a.txt"
printf '<b>autoindex B</b>\n' > "$ROOT/list/b.html"
printf 'secret noindex\n' > "$ROOT/noindex/secret.txt"

cat > "$CONF" <<CONFIG
server {
    listen ${PORT};
    root ./${ROOT};
    index index.html;

    error_page 404 ./${ROOT}/errors/404.html;
    error_page 403 ./${ROOT}/errors/403.html;
    error_page 405 ./${ROOT}/errors/405.html;

    location / {
        methods GET DELETE ;
    }

    location /files {
        root ./${ROOT}/files;
        methods GET DELETE ;
    }

    location /onlyget {
        root ./${ROOT}/files;
        methods GET ;
    }

    location /list {
        root ./${ROOT}/list;
        autoindex on;
        methods GET ;
    }

    location /noindex {
        root ./${ROOT}/noindex;
        autoindex off;
        methods GET ;
    }

    location /old {
        return 301 /new;
    }

    location /post-allowed {
        root ./${ROOT}/files;
        methods POST ;
    }
}
CONFIG

printf "Building project...\n"
make -j2 >/dev/null || { printf "make failed\n"; exit 1; }

printf "Starting ./webserv %s on port %s...\n" "$CONF" "$PORT"
./webserv "$CONF" > "$LOG" 2>&1 &
PID=$!

for i in $(seq 1 30); do
    if curl -sS --max-time 1 "http://127.0.0.1:${PORT}/" >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

if ! kill -0 "$PID" 2>/dev/null; then
    printf "Server died during startup. Log:\n"
    cat "$LOG"
    exit 1
fi

printf "Running HTTP response tests...\n"

expect_status "GET" "/" "200" "GET / returns 200"
resp="$(request GET /)"
expect_contains "$resp" "Content-Length: 14" "GET / has correct Content-Length"
expect_contains "$resp" "Connection: keep-alive" "Default HTTP/1.1 response keeps connection alive"
expect_contains "$resp" "Server: webserv" "Response includes Server header"
expect_contains "$resp" "ROOT INDEX OK" "GET / returns index body"

resp="$(request GET /files/hello.txt)"
expect_contains "$resp" "HTTP/1.1 200 OK" "Static text file returns 200 OK"
expect_contains "$resp" "Content-Type: text/plain" "Static .txt has text/plain Content-Type"
expect_contains "$resp" "Content-Length: 15" "Static file has correct Content-Length"
expect_contains "$resp" "hello response" "Static file body is correct"

resp="$(request GET /missing-page)"
expect_contains "$resp" "HTTP/1.1 404 Not Found" "Missing file returns 404"
expect_contains "$resp" "CUSTOM 404 PAGE" "404 uses configured error_page"
expect_contains "$resp" "Content-Length: 16" "404 error_page has correct Content-Length"

resp="$(request GET /old)"
expect_contains "$resp" "HTTP/1.1 301 Moved Permanently" "Redirect returns 301 Moved Permanently"
expect_contains "$resp" "Location: /new" "Redirect includes Location header"
expect_contains "$resp" "Content-Length: 0" "Redirect without body has Content-Length 0"

resp="$(request POST /onlyget/hello.txt)"
expect_contains "$resp" "HTTP/1.1 405 Method Not Allowed" "Disallowed method returns 405"
expect_contains "$resp" "Allow: GET" "405 includes Allow header"
expect_contains "$resp" "CUSTOM 405 PAGE" "405 uses configured error_page"

resp="$(request POST /post-allowed/hello.txt)"
expect_contains "$resp" "HTTP/1.1 501 Not Implemented" "Allowed but unimplemented POST returns 501"

resp="$(request GET /list/)"
expect_contains "$resp" "HTTP/1.1 200 OK" "Autoindex directory returns 200"
expect_contains "$resp" "Index of /list/" "Autoindex page title/body is generated"
expect_contains "$resp" "a.txt" "Autoindex lists a.txt"
expect_contains "$resp" "b.html" "Autoindex lists b.html"

resp="$(request GET /noindex/)"
expect_contains "$resp" "HTTP/1.1 403 Forbidden" "Directory without index and autoindex off returns 403"
expect_contains "$resp" "CUSTOM 403 PAGE" "403 uses configured error_page"

resp="$(request DELETE /files/delete-me.txt)"
expect_contains "$resp" "HTTP/1.1 204 No Content" "DELETE existing file returns 204"
expect_contains "$resp" "Content-Length: 0" "204 has Content-Length 0"
expect_not_contains "$resp" "delete me" "204 response has no body"

resp="$(request GET /files/delete-me.txt)"
expect_contains "$resp" "HTTP/1.1 404 Not Found" "Deleted file is gone"

resp="$(request GET / -H 'Connection: close')"
expect_contains "$resp" "Connection: close" "Connection: close request gets close response"

# ============================================================
# HTTP Parser tests
# Put this block before the final "Result: ... passed, ... failed"
# ============================================================

# Send a raw HTTP request directly using Python sockets.
# This avoids macOS/Linux differences in netcat options like -w, -N and -q.
raw_request() {
    raw="$1"
    RAW_HTTP_REQUEST="$raw" TEST_PORT="$PORT" python3 - <<'PY'
import os
import socket
import sys

host = "127.0.0.1"
port = int(os.environ["TEST_PORT"])

raw = os.environ["RAW_HTTP_REQUEST"]
raw = raw.encode("utf-8").decode("unicode_escape").encode("latin1")

try:
    s = socket.create_connection((host, port), timeout=3.0)
    s.settimeout(3.0)
    s.sendall(raw)

    chunks = []
    while True:
        try:
            data = s.recv(4096)
        except socket.timeout:
            break

        if not data:
            break

        chunks.append(data)

    s.close()
    sys.stdout.buffer.write(b"".join(chunks))
except Exception as e:
    sys.stderr.write("raw_request error: %s\n" % e)
PY
}

# Check only the status line of a raw HTTP response.
expect_raw_status() {
    raw="$1"
    expected="$2"
    label="$3"

    resp="$(raw_request "$raw")"
    first_line="$(printf "%s" "$resp" | sed -n '1p' | tr -d '\r')"

    case "$first_line" in
        "HTTP/1.1 ${expected}"*) ok "$label" ;;
        *)
            ko "$label"
            printf "Expected HTTP %s, got: %s\n" "$expected" "$first_line"
            printf "Full response:\n%s\n" "$resp"
            ;;
    esac
}

printf "\n=== HTTP Parser tests ===\n"

# Valid request with query string and mixed-case Host header.
# The parser should:
# - accept hOsT as Host because headers are case-insensitive
# - split the query string from the path
# - keep routing the request as GET /
resp="$(raw_request "GET /?name=Natalia&skill=C%2B%2B+webserv HTTP/1.1\r\nhOsT: localhost\r\nConnection: close\r\n\r\n")"
expect_contains "$resp" "HTTP/1.1 200 OK" "Parser accepts mixed-case Host header"
expect_contains "$resp" "ROOT INDEX OK" "Parser accepts URL with query string"
expect_contains "$resp" "Connection: close" "Parser reads Connection: close"

# HTTP/1.1 request without Host must be rejected.
expect_raw_status "GET / HTTP/1.1\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects missing Host header"

# Unsupported method must be rejected by the parser if your parser only accepts GET, POST and DELETE.
expect_raw_status "PUT / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects unsupported method"

# Unsupported HTTP version must be rejected by the parser.
expect_raw_status "GET / HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects unsupported HTTP version"

# Absolute URI is not accepted by this parser.
# The project expects origin-form paths like /index.html.
expect_raw_status "GET http://example.com/ HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects absolute URI in request line"

# POST without Content-Length or Transfer-Encoding must be rejected.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects POST without Content-Length or Transfer-Encoding"

# Duplicate Content-Length must be rejected.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nContent-Length: 0\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects duplicate Content-Length"

# Valid POST with Content-Length: 0 should pass the parser.
# The handler may still return 501 because POST behavior is not implemented yet.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n" \
                  "501" \
                  "Parser accepts POST with Content-Length: 0"

# Valid chunked POST should pass the parser.
# The handler may still return 501 because POST behavior is not implemented yet.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n" \
                  "501" \
                  "Parser accepts valid chunked body"

# Malformed chunked body must be rejected.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\nZ\r\nbad\r\n0\r\n\r\n" \
                  "400" \
                  "Parser rejects malformed chunked body"

printf "\nResult: %d passed, %d failed\n" "$PASS" "$FAIL"
if [ "$FAIL" -ne 0 ]; then
    printf "\nServer log:\n"
    tail -80 "$LOG"
    exit 1
fi
