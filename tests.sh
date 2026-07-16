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
COMPAT_PORT="${COMPAT_PORT:-$((PORT + 3))}"
COMPAT_CONF=".test_42_compat.conf"
COMPAT_ROOT=".test_42_compat_www"
COMPAT_LOG=".test_42_compat.log"
COMPAT_PID=""

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
        "$MULTI_OUT2" \
        "$COMPAT_CONF" \
        "$COMPAT_ROOT" \
        "$COMPAT_LOG"

    if [ -f "evil.txt" ] &&
       grep -Fq "UPLOAD CONTENT" "evil.txt" 2>/dev/null; then
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
expect_contains "$resp" "HTTP/1.1 404 Not Found" "Missing CGI script returns 404"

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


# ============================================================
# 42 tester compatibility continuation
#
# Reproduces the routes and checks observed in the external tester:
# - GET/POST/HEAD method handling
# - directory, extension and missing-path routing
# - CGI body echo and CGI header propagation
# - exact client_max_body_size boundary (100 / 101 / 200 bytes)
# - concurrent GET and CGI load
#
# Environment controls:
#   FT_CGI_BODY_SIZE=100000000   Functional CGI echo size (default: 100 MB)
#   FT_FULL_STRESS=1             Run 20 x 5000 and 128 x 50 GET tests
#   FT_HEAVY_CGI_STRESS=1        Run 20 x 5 CGI requests of 100 MB each
#   FT_STRESS_CGI_BODY_SIZE=...  Override concurrent CGI request size
# ============================================================

printf "\n=== 42 tester compatibility tests ===\n"

compat_cleanup() {
    if [ -n "${COMPAT_PID:-}" ] && kill -0 "$COMPAT_PID" 2>/dev/null; then
        kill "$COMPAT_PID" 2>/dev/null || true
        wait "$COMPAT_PID" 2>/dev/null || true
    fi
}
trap 'compat_cleanup; cleanup' EXIT
trap 'compat_cleanup; cleanup; exit 130' INT
trap 'compat_cleanup; cleanup; exit 143' TERM

compat_skip() {
    printf "\033[33m[SKIP]\033[0m %s\n" "$1"
}

compat_request() {
    method="$1"
    path="$2"
    shift 2

    case "$method" in
        POST)
            curl -sS -i --max-time 10 -X POST --data-binary "" \
                "$@" "http://127.0.0.1:${COMPAT_PORT}${path}"
            ;;
        HEAD)
            curl -sS -i --max-time 10 --head \
                "$@" "http://127.0.0.1:${COMPAT_PORT}${path}"
            ;;
        *)
            curl -sS -i --max-time 10 -X "$method" \
                "$@" "http://127.0.0.1:${COMPAT_PORT}${path}"
            ;;
    esac
}

expect_compat_status() {
    method="$1"
    path="$2"
    expected="$3"
    label="$4"

    resp="$(compat_request "$method" "$path" 2>&1 || true)"
    first_line="$(printf "%s" "$resp" | sed -n '1p' | tr -d '\r')"

    case "$first_line" in
        "HTTP/1.1 ${expected}"*) ok "$label" ;;
        *)
            ko "$label"
            printf "Expected HTTP %s, got: %s\nFull response:\n%s\n" \
                "$expected" "$first_line" "$resp"
            ;;
    esac
}

expect_compat_success() {
    method="$1"
    path="$2"
    label="$3"

    resp="$(compat_request "$method" "$path" 2>&1 || true)"
    first_line="$(printf "%s" "$resp" | sed -n '1p' | tr -d '\r')"
    status="$(printf "%s" "$first_line" | awk '{print $2}')"

    case "$status" in
        200|201|202|203|204|205|206) ok "$label" ;;
        *)
            ko "$label"
            printf "Expected HTTP 200-206, got: %s\nFull response:\n%s\n" \
                "$first_line" "$resp"
            ;;
    esac
}

rm -rf "$COMPAT_CONF" "$COMPAT_ROOT" "$COMPAT_LOG"
mkdir -p \
    "$COMPAT_ROOT/directory/nop" \
    "$COMPAT_ROOT/cgi_youpi" \
    "$COMPAT_ROOT/cgi_youpla" \
    "$COMPAT_ROOT/special_ok" \
    "$COMPAT_ROOT/post_body"

printf '42 ROOT INDEX OK\n' > "$COMPAT_ROOT/index.html"
printf '42 DIRECTORY INDEX OK\n' > "$COMPAT_ROOT/directory/index.html"
printf 'BAD EXTENSION FILE OK\n' > "$COMPAT_ROOT/directory/youpi.bad_extension"
printf 'NOP INDEX OK\n' > "$COMPAT_ROOT/directory/nop/index.html"
printf 'OTHER POUIC OK\n' > "$COMPAT_ROOT/directory/nop/other.pouic"
printf 'NOT HAPPY FILE OK\n' > "$COMPAT_ROOT/special_ok/index.html"

# CGI used by /directory/youpi.bla and /directory/youpla.bla.
# It echoes the request body byte-for-byte and exposes the test header
# in X-Secret-Seen when the server correctly builds CGI environment variables.
cat > "$COMPAT_ROOT/cgi_youpi/echo.py" <<'PYCGI42'
#!/usr/bin/env python3
import os
import sys

content_length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
secret = os.environ.get("HTTP_X_SECRET_HEADER_FOR_TEST", "")

out = sys.stdout.buffer
out.write(b"Content-Type: application/octet-stream\r\n")
out.write(("X-Echo-Length: %d\r\n" % content_length).encode("ascii"))
if secret:
    out.write(("X-Secret-Seen: %s\r\n" % secret).encode("ascii"))
out.write(b"\r\n")
out.flush()

remaining = content_length
while remaining > 0:
    chunk = sys.stdin.buffer.read(min(1024 * 1024, remaining))
    if not chunk:
        break
    out.write(chunk)
    out.flush()
    remaining -= len(chunk)
PYCGI42
chmod +x "$COMPAT_ROOT/cgi_youpi/echo.py"
cp "$COMPAT_ROOT/cgi_youpi/echo.py" "$COMPAT_ROOT/cgi_youpla/echo.py"
chmod +x "$COMPAT_ROOT/cgi_youpla/echo.py"

# CGI for the exact 100-byte request-body limit.
cat > "$COMPAT_ROOT/post_body/echo.py" <<'PYCGI42'
#!/usr/bin/env python3
import sys

body = sys.stdin.buffer.read()
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("X-Body-Length: %d\r\n" % len(body))
sys.stdout.write("\r\n")
sys.stdout.flush()
sys.stdout.buffer.write(body)
PYCGI42
chmod +x "$COMPAT_ROOT/post_body/echo.py"

cat > "$COMPAT_CONF" <<CONFIG42
server {
    listen ${COMPAT_PORT};
    root ./${COMPAT_ROOT};
    index index.html;
    client_max_body_size 120000000;

    location / {
        methods GET ;
    }

    location /directory {
        root ./${COMPAT_ROOT}/directory;
        index index.html;
        methods GET ;
    }

    location /directory/youpi.bla {
        root ./${COMPAT_ROOT}/cgi_youpi;
        index echo.py;
        methods GET POST ;
        cgi /usr/bin/python3;
        client_max_body_size 120000000;
    }

    location /directory/youpla.bla {
        root ./${COMPAT_ROOT}/cgi_youpla;
        index echo.py;
        methods GET POST ;
        cgi /usr/bin/python3;
        client_max_body_size 120000000;
    }

    location /directory/Yeah/not_happy.bad_extension {
        root ./${COMPAT_ROOT}/special_ok;
        index index.html;
        methods GET ;
    }

    location /post_body {
        root ./${COMPAT_ROOT}/post_body;
        index echo.py;
        methods POST ;
        cgi /usr/bin/python3;
        client_max_body_size 100;
    }
}
CONFIG42

./webserv "$COMPAT_CONF" > "$COMPAT_LOG" 2>&1 &
COMPAT_PID=$!

compat_started=0
for i in $(seq 1 50); do
    if curl -sS --max-time 1 \
        "http://127.0.0.1:${COMPAT_PORT}/" >/dev/null 2>&1; then
        compat_started=1
        break
    fi

    if ! kill -0 "$COMPAT_PID" 2>/dev/null; then
        break
    fi
    sleep 0.2
done

if [ "$compat_started" = "1" ] && kill -0 "$COMPAT_PID" 2>/dev/null; then
    ok "42 compatibility server starts"
else
    ko "42 compatibility server starts"
    printf "Compatibility server log:\n"
    cat "$COMPAT_LOG" 2>/dev/null || true
fi

if [ "$compat_started" = "1" ]; then
    printf "\n--- Methods, files and routing ---\n"

    expect_compat_success "GET" "/" \
        "42: GET / returns HTTP 200-206"
    expect_compat_status "POST" "/" "405" \
        "42: POST / returns 405 Method Not Allowed"
    expect_compat_status "HEAD" "/" "405" \
        "42: HEAD / returns 405 Method Not Allowed"

    expect_compat_success "GET" "/directory" \
        "42: GET /directory succeeds"
    expect_compat_success "GET" "/directory/youpi.bad_extension" \
        "42: GET existing .bad_extension file succeeds"
    expect_compat_success "GET" "/directory/youpi.bla" \
        "42: GET /directory/youpi.bla succeeds"
    expect_compat_status "GET" "/directory/oulalala" "404" \
        "42: missing /directory/oulalala returns 404"
    expect_compat_success "GET" "/directory/nop" \
        "42: GET /directory/nop succeeds"
    expect_compat_success "GET" "/directory/nop/" \
        "42: GET /directory/nop/ succeeds"
    expect_compat_success "GET" "/directory/nop/other.pouic" \
        "42: existing other.pouic succeeds"
    expect_compat_status "GET" "/directory/nop/other.pouac" "404" \
        "42: missing other.pouac returns 404"
    expect_compat_status "GET" "/directory/Yeah" "404" \
        "42: GET /directory/Yeah returns 404"
    expect_compat_success "GET" "/directory/Yeah/not_happy.bad_extension" \
        "42: nested not_happy.bad_extension succeeds"

    printf "\n--- CGI body and header propagation ---\n"

    compat_cgi_check() {
        path="$1"
        size="$2"
        secret="$3"
        label="$4"

        if COMPAT_TEST_PORT="$COMPAT_PORT" \
           COMPAT_TEST_PATH="$path" \
           COMPAT_TEST_SIZE="$size" \
           COMPAT_TEST_SECRET="$secret" \
           python3 - <<'PY42HTTP'
import http.client
import os
import sys

port = int(os.environ["COMPAT_TEST_PORT"])
path = os.environ["COMPAT_TEST_PATH"]
size = int(os.environ["COMPAT_TEST_SIZE"])
secret = os.environ.get("COMPAT_TEST_SECRET", "")

pattern = b"0123456789abcdef"
payload = (pattern * ((size + len(pattern) - 1) // len(pattern)))[:size]

headers = {
    "Content-Type": "application/octet-stream",
    "Content-Length": str(size),
    "Connection": "close",
}
if secret:
    headers["X-SECRET-HEADER-FOR-TEST"] = secret

conn = http.client.HTTPConnection("127.0.0.1", port, timeout=180)
try:
    conn.request("POST", path, body=payload, headers=headers)
    response = conn.getresponse()
    response_body = response.read()

    if response.status < 200 or response.status > 206:
        raise RuntimeError("expected HTTP 200-206, got %d" % response.status)

    if response_body != payload:
        raise RuntimeError(
            "CGI body mismatch: sent %d bytes, received %d bytes"
            % (len(payload), len(response_body))
        )

    echo_length = response.getheader("X-Echo-Length")
    if echo_length != str(size):
        raise RuntimeError(
            "X-Echo-Length mismatch: expected %d, got %r"
            % (size, echo_length)
        )

    if secret:
        seen = response.getheader("X-Secret-Seen")
        if seen != secret:
            raise RuntimeError(
                "CGI header propagation failed: expected %r, got %r"
                % (secret, seen)
            )
except Exception as exc:
    sys.stderr.write("%s\n" % exc)
    sys.exit(1)
finally:
    conn.close()
PY42HTTP
        then
            ok "$label"
        else
            ko "$label"
        fi
    }

    FT_CGI_BODY_SIZE="${FT_CGI_BODY_SIZE:-100000000}"

    compat_cgi_check \
        "/directory/youpi.bla" \
        "$FT_CGI_BODY_SIZE" \
        "" \
        "42: CGI youpi.bla echoes ${FT_CGI_BODY_SIZE} bytes"

    compat_cgi_check \
        "/directory/youpla.bla" \
        "$FT_CGI_BODY_SIZE" \
        "" \
        "42: CGI youpla.bla echoes ${FT_CGI_BODY_SIZE} bytes"

    compat_cgi_check \
        "/directory/youpi.bla" \
        "100000" \
        "7" \
        "42: CGI receives X-SECRET-HEADER-FOR-TEST"

    printf "\n--- Exact request-body limit ---\n"

    compat_body_limit_check() {
        size="$1"
        expected="$2"
        label="$3"

        if COMPAT_TEST_PORT="$COMPAT_PORT" \
           COMPAT_TEST_SIZE="$size" \
           COMPAT_EXPECTED_STATUS="$expected" \
           python3 - <<'PY42LIMIT'
import http.client
import os
import sys

port = int(os.environ["COMPAT_TEST_PORT"])
size = int(os.environ["COMPAT_TEST_SIZE"])
expected = int(os.environ["COMPAT_EXPECTED_STATUS"])
payload = b"x" * size

conn = http.client.HTTPConnection("127.0.0.1", port, timeout=20)
try:
    conn.request(
        "POST",
        "/post_body",
        body=payload,
        headers={
            "Content-Type": "application/octet-stream",
            "Content-Length": str(size),
            "Connection": "close",
        },
    )
    response = conn.getresponse()
    response.read()

    if expected == 200:
        valid = 200 <= response.status <= 206
    else:
        valid = response.status == expected

    if not valid:
        raise RuntimeError(
            "sent %d bytes: expected %s, got HTTP %d"
            % (
                size,
                "200-206" if expected == 200 else str(expected),
                response.status,
            )
        )
except Exception as exc:
    sys.stderr.write("%s\n" % exc)
    sys.exit(1)
finally:
    conn.close()
PY42LIMIT
        then
            ok "$label"
        else
            ko "$label"
        fi
    }

    compat_body_limit_check 0   200 \
        "42: POST /post_body with 0 bytes succeeds"
    compat_body_limit_check 100 200 \
        "42: POST /post_body with exactly 100 bytes succeeds"
    compat_body_limit_check 101 413 \
        "42: POST /post_body with 101 bytes returns 413"
    compat_body_limit_check 200 413 \
        "42: POST /post_body with 200 bytes returns 413"

    printf "\n--- Concurrent GET load ---\n"

    compat_get_load() {
        workers="$1"
        repeats="$2"
        path="$3"
        label="$4"

        if COMPAT_TEST_PORT="$COMPAT_PORT" \
           COMPAT_WORKERS="$workers" \
           COMPAT_REPEATS="$repeats" \
           COMPAT_TEST_PATH="$path" \
           python3 - <<'PY42LOAD'
import concurrent.futures
import http.client
import os
import sys

port = int(os.environ["COMPAT_TEST_PORT"])
workers = int(os.environ["COMPAT_WORKERS"])
repeats = int(os.environ["COMPAT_REPEATS"])
path = os.environ["COMPAT_TEST_PATH"]

def run_worker(_worker_id):
    completed = 0
    conn = None

    try:
        conn = http.client.HTTPConnection("127.0.0.1", port, timeout=20)

        for _ in range(repeats):
            try:
                conn.request(
                    "GET",
                    path,
                    headers={"Host": "localhost", "Connection": "keep-alive"},
                )
                response = conn.getresponse()
                response.read()

                if response.status < 200 or response.status > 206:
                    raise RuntimeError("HTTP %d" % response.status)

                completed += 1
            except Exception:
                try:
                    conn.close()
                except Exception:
                    pass
                conn = http.client.HTTPConnection(
                    "127.0.0.1", port, timeout=20
                )
                raise
    finally:
        if conn is not None:
            conn.close()

    return completed

try:
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=workers
    ) as executor:
        results = list(executor.map(run_worker, range(workers)))

    total = sum(results)
    expected = workers * repeats
    if total != expected:
        raise RuntimeError(
            "completed %d/%d requests" % (total, expected)
        )
except Exception as exc:
    sys.stderr.write("%s\n" % exc)
    sys.exit(1)
PY42LOAD
        then
            ok "$label"
        else
            ko "$label"
        fi
    }

    # This one is small enough to run exactly every time.
    compat_get_load 5 15 "/" \
        "42 load: 5 workers x 15 GET / = 75 requests"

    if [ "${FT_FULL_STRESS:-0}" = "1" ]; then
        compat_get_load 20 5000 "/" \
            "42 load: 20 workers x 5000 GET / = 100000 requests"
        compat_get_load 128 50 "/directory/nop" \
            "42 load: 128 workers x 50 GET /directory/nop = 6400 requests"
    else
        compat_get_load 20 100 "/" \
            "42 load scaled: 20 workers x 100 GET / = 2000 requests"
        compat_get_load 32 20 "/directory/nop" \
            "42 load scaled: 32 workers x 20 GET /directory/nop = 640 requests"
        compat_skip \
            "Set FT_FULL_STRESS=1 for the exact 100000 + 6400 GET load"
    fi

    printf "\n--- Concurrent CGI load ---\n"

    compat_cgi_load() {
        workers="$1"
        repeats="$2"
        size="$3"
        label="$4"

        if COMPAT_TEST_PORT="$COMPAT_PORT" \
           COMPAT_WORKERS="$workers" \
           COMPAT_REPEATS="$repeats" \
           COMPAT_TEST_SIZE="$size" \
           python3 - <<'PY42CGILOAD'
import concurrent.futures
import http.client
import os
import sys

port = int(os.environ["COMPAT_TEST_PORT"])
workers = int(os.environ["COMPAT_WORKERS"])
repeats = int(os.environ["COMPAT_REPEATS"])
size = int(os.environ["COMPAT_TEST_SIZE"])

class RepeatedBody:
    def __init__(self, total):
        self.remaining = total

    def read(self, amount=-1):
        if self.remaining <= 0:
            return b""
        if amount is None or amount < 0:
            amount = self.remaining
        amount = min(amount, self.remaining)
        self.remaining -= amount
        return b"Z" * amount

def run_worker(_worker_id):
    completed = 0

    for _ in range(repeats):
        conn = http.client.HTTPConnection(
            "127.0.0.1", port, timeout=240
        )
        try:
            conn.request(
                "POST",
                "/directory/youpi.bla",
                body=RepeatedBody(size),
                headers={
                    "Content-Type": "application/octet-stream",
                    "Content-Length": str(size),
                    "Connection": "close",
                },
            )
            response = conn.getresponse()

            if response.status < 200 or response.status > 206:
                response.read()
                raise RuntimeError("HTTP %d" % response.status)

            received = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                if chunk != b"Z" * len(chunk):
                    raise RuntimeError("CGI response body content mismatch")
                received += len(chunk)

            if received != size:
                raise RuntimeError(
                    "body mismatch: expected %d bytes, got %d"
                    % (size, received)
                )

            completed += 1
        finally:
            conn.close()

    return completed

try:
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=workers
    ) as executor:
        results = list(executor.map(run_worker, range(workers)))

    total = sum(results)
    expected = workers * repeats
    if total != expected:
        raise RuntimeError(
            "completed %d/%d CGI requests" % (total, expected)
        )
except Exception as exc:
    sys.stderr.write("%s\n" % exc)
    sys.exit(1)
PY42CGILOAD
        then
            ok "$label"
        else
            ko "$label"
        fi
    }

    if [ "${FT_HEAVY_CGI_STRESS:-0}" = "1" ]; then
        FT_STRESS_CGI_BODY_SIZE="${FT_STRESS_CGI_BODY_SIZE:-100000000}"
        compat_cgi_load 20 5 "$FT_STRESS_CGI_BODY_SIZE" \
            "42 load: 20 workers x 5 CGI POSTs of ${FT_STRESS_CGI_BODY_SIZE} bytes"
    else
        FT_STRESS_CGI_BODY_SIZE="${FT_STRESS_CGI_BODY_SIZE:-1000000}"
        compat_cgi_load 5 2 "$FT_STRESS_CGI_BODY_SIZE" \
            "42 load scaled: 5 workers x 2 CGI POSTs of ${FT_STRESS_CGI_BODY_SIZE} bytes"
        compat_skip \
            "Set FT_HEAVY_CGI_STRESS=1 for 100 CGI POSTs of 100 MB (~10 GB uploaded and echoed)"
    fi
fi

compat_cleanup
COMPAT_PID=""

printf "\nResult: %d passed, %d failed\n" "$PASS" "$FAIL"
if [ "$FAIL" -ne 0 ]; then
    printf "\nServer log:\n"
    tail -80 "$LOG"
    exit 1
fi
