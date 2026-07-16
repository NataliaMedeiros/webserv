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

MULTI_PORT1="${MULTI_PORT1:-$((PORT + 1))}"
MULTI_PORT2="${MULTI_PORT2:-$((PORT + 2))}"
MULTI_CONF=".test_multi_port.conf"
MULTI_ROOT1=".test_multi_port_1"
MULTI_ROOT2=".test_multi_port_2"
MULTI_LOG=".test_multi_port.log"
MULTI_OUT1=".test_multi_port_1.out"
MULTI_OUT2=".test_multi_port_2.out"
MULTI_PID=""

# Delete only files/folders created by this script.
clean_test_artifacts() {
    rm -rf \
        "$CONF" \
        "$ROOT" \
        "$LOG" \
        "$MULTI_CONF" \
        "$MULTI_ROOT1" \
        "$MULTI_ROOT2" \
        "$MULTI_LOG" \
        "$MULTI_OUT1" \
        "$MULTI_OUT2"

    # Only remove this if a broken path-traversal test created it.
    if [ -f "evil.txt" ] && grep -Fq "UPLOAD CONTENT" "evil.txt" 2>/dev/null; then
        rm -f "evil.txt"
    fi
}

# This function only kills running test servers.
# Temporary files are removed manually with: ./tests.sh clean
cleanup() {
    if [ -n "${PID}" ] && kill -0 "${PID}" 2>/dev/null; then
        kill "${PID}" 2>/dev/null || true
        wait "${PID}" 2>/dev/null || true
    fi
    if [ -n "${MULTI_PID}" ] && kill -0 "${MULTI_PID}" 2>/dev/null; then
        kill "${MULTI_PID}" 2>/dev/null || true
        wait "${MULTI_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM

if [ "${1:-}" = "clean" ]; then
    clean_test_artifacts
    printf "Test artifacts removed.\n"
    exit 0
fi

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


# ============================================================
# Merge sanity checks: catches using old integration stubs instead
# of the real HttpResponse implementation from feature-http-response.
# Put this after helper functions and before make/build.
# ============================================================
expect_makefile_contains_src() {
    needle="$1"
    label="$2"
    if grep -v '^[[:space:]]*#' Makefile | grep -Fq "$needle"; then
        ok "$label"
    else
        ko "$label"
        printf "Expected Makefile SRCS to contain: %s\n" "$needle"
    fi
}

expect_makefile_not_contains_src() {
    needle="$1"
    label="$2"
    if grep -v '^[[:space:]]*#' Makefile | grep -Fq "$needle"; then
        ko "$label"
        printf "Makefile should not compile old stub source: %s\n" "$needle"
    else
        ok "$label"
    fi
}

expect_file_content() {
    file="$1"
    needle="$2"
    label="$3"
    if [ -f "$file" ] && grep -Fq "$needle" "$file"; then
        ok "$label"
    else
        ko "$label"
        printf "Expected file %s to contain: %s\n" "$file" "$needle"
        [ -f "$file" ] && printf "Current file content:\n" && cat "$file"
    fi
}

expect_file_not_exists() {
    file="$1"
    label="$2"
    if [ ! -e "$file" ]; then
        ok "$label"
    else
        ko "$label"
        printf "File should not exist: %s\n" "$file"
    fi
}

printf "Running merge sanity checks...\n"
expect_makefile_contains_src "src/HttpResponse.cpp" "Merge uses real HttpResponse.cpp"
expect_makefile_not_contains_src "src/stubs.cpp" "Merge does not compile old integration stubs.cpp"

printf "Preparing test files...\n"
rm -rf "$ROOT" "$CONF" "$LOG"
mkdir -p "$ROOT/errors" "$ROOT/files" "$ROOT/list" "$ROOT/noindex" "$ROOT/uploads" "$ROOT/cgi"
printf 'ROOT INDEX OK\n' > "$ROOT/index.html"
printf 'hello response\n' > "$ROOT/files/hello.txt"
printf 'delete me\n' > "$ROOT/files/delete-me.txt"
printf 'CUSTOM 404 PAGE\n' > "$ROOT/errors/404.html"
printf 'CUSTOM 403 PAGE\n' > "$ROOT/errors/403.html"
printf 'CUSTOM 405 PAGE\n' > "$ROOT/errors/405.html"
printf 'autoindex A\n' > "$ROOT/list/a.txt"
printf '<b>autoindex B</b>\n' > "$ROOT/list/b.html"
printf 'secret noindex\n' > "$ROOT/noindex/secret.txt"
printf 'UPLOAD CONTENT
' > "$ROOT/files/upload-source.txt"
cat > "$ROOT/cgi/hello.py" <<'PYCGI'
#!/usr/bin/env python3
print("Content-Type: text/html")
print()
print("<h1>Hello CGI</h1>")
PYCGI
chmod +x "$ROOT/cgi/hello.py"

cat > "$ROOT/cgi/echo_body.py" <<'PYCGI'
#!/usr/bin/env python3
import sys

body = sys.stdin.read()

print("Content-Type: text/plain")
print()
print(body)
PYCGI
chmod +x "$ROOT/cgi/echo_body.py"

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

    location /upload {
        root ./${ROOT};
        upload_dir ./${ROOT}/uploads;
        methods POST ;
    }

    location /cgi {
        root ./${ROOT}/cgi;
        methods GET POST ;
        cgi /usr/bin/python3;
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
expect_contains "$resp" "HTTP/1.1 200 OK" "Allowed but unimplemented POST returns 200"

resp="$(request GET /list/)"
expect_contains "$resp" "HTTP/1.1 200 OK" "Autoindex directory returns 200"
expect_contains "$resp" "Index of /list/" "Autoindex page title/body is generated"
expect_contains "$resp" "a.txt" "Autoindex lists a.txt"
expect_contains "$resp" "b.html" "Autoindex lists b.html"

resp="$(request GET /noindex/)"
expect_contains "$resp" "HTTP/1.1 404 Not Found" "Directory without index and autoindex off returns 404"
expect_contains "$resp" "CUSTOM 404 PAGE" "404 uses configured error_page"

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
# The handler may still return 200 because POST behavior is not implemented yet.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n" \
                  "200" \
                  "Parser accepts POST with Content-Length: 0"

# Valid chunked POST should pass the parser.
# The handler may still return 200 because POST behavior is not implemented yet.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n" \
                  "200" \
                  "Parser accepts valid chunked body"

# Malformed chunked body must be rejected.
expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\nZ\r\nbad\r\n0\r\n\r\n" \
                  "400" \
                  "Parser rejects malformed chunked body"

# ============================================================
# Extra integration/security tests
# Put this block after the HTTP Parser tests and before the final
# "Result: ... passed, ... failed" block.
# ============================================================

printf "\n=== Extra integration/security tests ===\n"

# Helper: check that a response does NOT have a specific status code.
expect_not_status_in_response() {
    resp="$1"
    unexpected="$2"
    label="$3"

    first_line="$(printf "%s" "$resp" | sed -n '1p' | tr -d '\r')"

    case "$first_line" in
        "HTTP/1.1 ${unexpected}"*)
            ko "$label"
            printf "Did not expect HTTP %s, got: %s\nFull response:\n%s\n" "$unexpected" "$first_line" "$resp"
            ;;
        *)
            ok "$label"
            ;;
    esac
}

# Helper: check how many times a header appears.
# Useful to catch duplicate Content-Length.
expect_header_count() {
    resp="$1"
    header="$2"
    expected_count="$3"
    label="$4"

    count="$(printf "%s" "$resp" | tr -d '\r' | grep -i "^${header}:" | wc -l | tr -d ' ')"

    if [ "$count" = "$expected_count" ]; then
        ok "$label"
    else
        ko "$label"
        printf "Expected header '%s' to appear %s time(s), got %s\nFull response:\n%s\n" "$header" "$expected_count" "$count" "$resp"
    fi
}

# ------------------------------------------------------------
# Router prefix-boundary tests
# ------------------------------------------------------------

# If the router is correct, /filesabc must NOT match location /files.
# This file would be served only if /filesabc incorrectly matched /files.
printf 'SHOULD NOT MATCH FILES LOCATION\n' > "$ROOT/files/abc"

resp="$(request GET /filesabc)"
expect_contains "$resp" "HTTP/1.1 404 Not Found" "Router does not match /filesabc as /files"
expect_not_contains "$resp" "SHOULD NOT MATCH FILES LOCATION" "Router prefix boundary prevents wrong file from being served"

# Same idea for /onlygetextra: it must not match location /onlyget.
resp="$(request POST /onlygetextra/hello.txt)"
expect_contains "$resp" "HTTP/1.1 405 Method Not Allowed" "Router falls back to / for /onlygetextra"
expect_contains "$resp" "Allow: GET, DELETE" "Router did not match /onlyget for /onlygetextra"

# ------------------------------------------------------------
# Path traversal tests
# ------------------------------------------------------------

# These requests must never return 200 OK.
# Depending on your design, 400, 403 or 404 are acceptable.
resp="$(request GET /../Makefile --path-as-is)"
expect_not_status_in_response "$resp" "200" "Path traversal /../Makefile is not served"

resp="$(request GET /files/../errors/404.html --path-as-is)"
expect_not_status_in_response "$resp" "200" "Path traversal through location root is not served"

resp="$(request GET /%2e%2e/Makefile)"
expect_not_status_in_response "$resp" "200" "Encoded path traversal is not served"

# ------------------------------------------------------------
# Duplicate header checks
# ------------------------------------------------------------

resp="$(request GET /)"
expect_header_count "$resp" "Content-Length" "1" "GET / has exactly one Content-Length header"

resp="$(request GET /old)"
expect_header_count "$resp" "Content-Length" "1" "Redirect has exactly one Content-Length header"

resp="$(request GET /missing-page)"
expect_header_count "$resp" "Content-Length" "1" "Error page has exactly one Content-Length header"

# ------------------------------------------------------------
# More malformed parser requests
# These use raw_request(), so this block must come after raw_request is defined.
# ------------------------------------------------------------

expect_raw_status "GET / HTTP/1.1\r\nHost localhost\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects header without colon"

expect_raw_status "GET / HTTP/1.1\r\nHost: localhost\r\nBrokenHeader\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects malformed header line even with valid Host"

expect_raw_status "GET / HTTP/1.1\r\nHost:\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects empty Host header"

expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects non-numeric Content-Length"

expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects negative Content-Length"

expect_raw_status "POST /post-allowed/hello.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\nConnection: close\r\n\r\n" \
                  "400" \
                  "Parser rejects unsupported Transfer-Encoding"

# ------------------------------------------------------------
# Keep-alive / pipelined requests
# ------------------------------------------------------------

keepalive_two_requests() {
    TEST_PORT="$PORT" python3 - <<'PY'
import socket
import sys

host = "127.0.0.1"
port = int(__import__("os").environ["TEST_PORT"])

def read_one_response(sock):
    data = b""

    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            return data
        data += chunk

    header, rest = data.split(b"\r\n\r\n", 1)
    content_length = 0

    for line in header.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            content_length = int(line.split(b":", 1)[1].strip())

    while len(rest) < content_length:
        chunk = sock.recv(4096)
        if not chunk:
            break
        rest += chunk

    return header + b"\r\n\r\n" + rest[:content_length]

try:
    s = socket.create_connection((host, port), timeout=3.0)
    s.settimeout(3.0)

    s.sendall(
        b"GET / HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"\r\n"
    )

    first = read_one_response(s)

    s.sendall(
        b"GET /files/hello.txt HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )

    second = read_one_response(s)

    s.close()

    sys.stdout.buffer.write(first + b"\n---SECOND RESPONSE---\n" + second)
except Exception as e:
    sys.stderr.write("keepalive_two_requests error: %s\n" % e)
PY
}

resp="$(keepalive_two_requests)"
expect_contains "$resp" "ROOT INDEX OK" "Keep-alive first request returns body"
expect_contains "$resp" "hello response" "Keep-alive second request returns body"


# ============================================================
# Upload + CGI integration tests from the integration branch.
# These verify that the merge kept Router/Handler/FileSystem features
# while using the real HttpResponse serializer.
# ============================================================

printf "\n=== Upload + CGI merge tests ===\n"

resp="$(curl -sS -i --max-time 3 \
    -F "file=@${ROOT}/files/upload-source.txt;filename=merge-upload.txt" \
    "http://127.0.0.1:${PORT}/upload")"
expect_contains "$resp" "HTTP/1.1 201 Created" "Upload returns 201 Created"
expect_header_count "$resp" "Content-Length" "1" "Upload has exactly one Content-Length header"
expect_file_content "$ROOT/uploads/merge-upload.txt" "UPLOAD CONTENT" "Upload writes file content to upload_dir"

resp="$(curl -sS -i --max-time 3 \
    -F "file=@${ROOT}/files/upload-source.txt;filename=../../evil.txt" \
    "http://127.0.0.1:${PORT}/upload")"
expect_contains "$resp" "HTTP/1.1 400 " "Upload rejects path traversal filename"
expect_file_not_exists "$ROOT/../evil.txt" "Upload path traversal does not create outside file"

resp="$(curl -sS -i --max-time 3 \
    -F "file=@${ROOT}/files/upload-source.txt;filename=" \
    "http://127.0.0.1:${PORT}/upload")"
expect_contains "$resp" "HTTP/1.1 400 " "Upload rejects empty filename"

resp="$(request GET /cgi/hello.py)"
expect_contains "$resp" "HTTP/1.1 200 OK" "CGI GET returns 200 OK"
expect_contains "$resp" "Hello CGI" "CGI response body is returned"
expect_header_count "$resp" "Content-Length" "1" "CGI response has exactly one Content-Length header"

resp="$(request GET /cgi/missing.py)"
expect_contains "$resp" "HTTP/1.1 502 Bad Gateway" "Missing CGI script returns 502"

resp="$(raw_request "POST /cgi/echo_body.py HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n")"

expect_contains "$resp" "HTTP/1.1 200 OK" "CGI accepts chunked POST request"
expect_contains "$resp" "Wikipedia" "CGI receives unchunked request body"
expect_not_contains "$resp" "4\r\nWiki" "CGI body does not contain raw chunk size"



# ============================================================
# Multi-port merge tests
# These verify that one webserv process can bind more than one
# listen port and route each port to the correct server root.
# ============================================================

printf "\n=== Multi-port merge tests ===\n"

rm -rf "$MULTI_CONF" "$MULTI_ROOT1" "$MULTI_ROOT2" "$MULTI_LOG" "$MULTI_OUT1" "$MULTI_OUT2"
mkdir -p "$MULTI_ROOT1" "$MULTI_ROOT2"

printf "MULTI PORT %s ROOT OK\n" "$MULTI_PORT1" > "$MULTI_ROOT1/index.html"
printf "MULTI PORT %s ROOT OK\n" "$MULTI_PORT2" > "$MULTI_ROOT2/index.html"

cat > "$MULTI_CONF" <<CONFIG
server {
    listen ${MULTI_PORT1};
    root ./${MULTI_ROOT1};
    index index.html;

    location / {
        methods GET ;
    }
}

server {
    listen ${MULTI_PORT2};
    root ./${MULTI_ROOT2};
    index index.html;

    location / {
        methods GET ;
    }
}
CONFIG

./webserv "$MULTI_CONF" > "$MULTI_LOG" 2>&1 &
MULTI_PID=$!

for i in $(seq 1 30); do
    ok1=0
    ok2=0
    curl -sS --max-time 1 "http://127.0.0.1:${MULTI_PORT1}/" >/dev/null 2>&1 && ok1=1
    curl -sS --max-time 1 "http://127.0.0.1:${MULTI_PORT2}/" >/dev/null 2>&1 && ok2=1
    if [ "$ok1" = "1" ] && [ "$ok2" = "1" ]; then
        break
    fi
    sleep 0.2
done

if kill -0 "$MULTI_PID" 2>/dev/null; then
    ok "Multi-port server process stays alive"
else
    ko "Multi-port server process stays alive"
    printf "Multi-port server died during startup. Log:\n"
    cat "$MULTI_LOG" 2>/dev/null || true
fi

resp="$(curl -sS -i --max-time 3 "http://127.0.0.1:${MULTI_PORT1}/")"
expect_contains "$resp" "HTTP/1.1 200 OK" "Multi-port: first port returns 200"
expect_contains "$resp" "MULTI PORT ${MULTI_PORT1} ROOT OK" "Multi-port: first port serves first root"
expect_not_contains "$resp" "MULTI PORT ${MULTI_PORT2} ROOT OK" "Multi-port: first port does not serve second root"
expect_header_count "$resp" "Content-Length" "1" "Multi-port: first port has exactly one Content-Length header"

resp="$(curl -sS -i --max-time 3 "http://127.0.0.1:${MULTI_PORT2}/")"
expect_contains "$resp" "HTTP/1.1 200 OK" "Multi-port: second port returns 200"
expect_contains "$resp" "MULTI PORT ${MULTI_PORT2} ROOT OK" "Multi-port: second port serves second root"
expect_not_contains "$resp" "MULTI PORT ${MULTI_PORT1} ROOT OK" "Multi-port: second port does not serve first root"
expect_header_count "$resp" "Content-Length" "1" "Multi-port: second port has exactly one Content-Length header"

# Quick parallel smoke test: both listening sockets should answer while
# the same webserv process is running.
rm -f "$MULTI_OUT1" "$MULTI_OUT2"
(curl -sS -i --max-time 3 "http://127.0.0.1:${MULTI_PORT1}/" > "$MULTI_OUT1") &
CURL_PID1=$!
(curl -sS -i --max-time 3 "http://127.0.0.1:${MULTI_PORT2}/" > "$MULTI_OUT2") &
CURL_PID2=$!
wait "$CURL_PID1" 2>/dev/null || true
wait "$CURL_PID2" 2>/dev/null || true

resp1="$(cat "$MULTI_OUT1" 2>/dev/null || true)"
resp2="$(cat "$MULTI_OUT2" 2>/dev/null || true)"
expect_contains "$resp1" "MULTI PORT ${MULTI_PORT1} ROOT OK" "Multi-port: parallel request to first port works"
expect_contains "$resp2" "MULTI PORT ${MULTI_PORT2} ROOT OK" "Multi-port: parallel request to second port works"

if [ -n "$MULTI_PID" ] && kill -0 "$MULTI_PID" 2>/dev/null; then
    kill "$MULTI_PID" 2>/dev/null || true
    wait "$MULTI_PID" 2>/dev/null || true
fi

printf "\nResult: %d passed, %d failed\n" "$PASS" "$FAIL"
if [ "$FAIL" -ne 0 ]; then
    printf "\nServer log:\n"
    tail -80 "$LOG"
    exit 1
fi
