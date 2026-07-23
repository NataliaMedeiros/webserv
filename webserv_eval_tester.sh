#!/bin/bash

# ==========================================================
# 42 Webserv Evaluation Tester (FIXED)
# Automated checks + Siege stress tests
#
# Fixed to match this project's actual configuration and
# file layout. Must be run against: configs/default_with_error.config
# See webserv_eval_tester_instructions.md for setup steps.
# ==========================================================

HOST="localhost"
PORT="8080"
URL="http://$HOST:$PORT"

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
RESET="\033[0m"

PASS=0
FAIL=0


check()
{
    NAME=$1
    CMD=$2
    EXPECT=$3

    echo
    echo "-----------------------------------------"
    echo "$NAME"
    echo "-----------------------------------------"

    RESULT=$(eval "$CMD")

    if echo "$RESULT" | grep -q "$EXPECT"
    then
        echo -e "${GREEN}PASS${RESET}"
        PASS=$((PASS+1))
    else
        echo -e "${RED}FAIL${RESET}"
        echo "Expected: $EXPECT"
        echo "Got:"
        echo "$RESULT"
        FAIL=$((FAIL+1))
    fi
}


echo "========================================="
echo "  42 Webserv Evaluation Tester (fixed)"
echo "========================================="


# ----------------------------------------------------------
# SERVER CHECK
# ----------------------------------------------------------

echo
echo "Checking server..."

if curl -s "$URL" > /dev/null
then
    echo -e "${GREEN}Server online${RESET}"
else
    echo -e "${RED}Server not reachable${RESET}"
    exit 1
fi


# ----------------------------------------------------------
# BASIC HTTP METHODS
# ----------------------------------------------------------

check \
"GET request should return 200" \
"curl -i -s $URL" \
"200"


check \
"Unknown method should return error" \
"curl -i -s -X RANDOM $URL" \
"400"


check \
"POST request" \
"curl -i -s -X POST -d 'BODY IS HERE' $URL" \
"200"


# FIXED: DELETE now targets a file we create fresh in www/ root,
# instead of a non-existent /test.txt (root-level test.txt never existed,
# real fixture files live under www/files/).
echo "42 Webserv delete test" > www/delete_me.txt

check \
"DELETE request" \
"curl -i -s -X DELETE $URL/delete_me.txt" \
"204"


# ----------------------------------------------------------
# ERROR PAGES
# ----------------------------------------------------------

check \
"404 custom error page" \
"curl -i -s $URL/non_existing_file" \
"404"


# ----------------------------------------------------------
# BODY SIZE LIMIT
# ----------------------------------------------------------

echo
echo "Testing client body limit"

# FIXED: write the big body to a temp file and use --data-binary @file
# instead of passing a 1MB string as a shell argument, which previously
# hit the OS "Argument list too long" limit and never reached curl at all.
python3 -c "print('A' * 15000000, end='')" > /tmp/eval_big_body.txt

check \
"Body over the /upload limit (10M) returns 413" \
"curl -i -s -X POST --data-binary @/tmp/eval_big_body.txt $URL/upload" \
"413"


# ----------------------------------------------------------
# FILE UPLOAD / DOWNLOAD
# ----------------------------------------------------------

echo "Upload test file"

echo "42 Webserv upload test" > /tmp/eval_test_upload.txt


# FIXED: use a real multipart/form-data upload (-F), matching how a
# browser <input type="file"> actually submits a file. The old test used
# --data-binary (a raw, non-multipart body), which this server correctly
# rejects with 400 "Malformed multipart body", that was never a server bug.
# FIXED: expect 201 Created (the correct status for a newly created
# resource), not 200 OK.
check \
"Upload file (multipart/form-data)" \
"curl -i -s -X POST -F 'file=@/tmp/eval_test_upload.txt' $URL/upload" \
"201"


# FIXED: uploaded files are served back from /uploads/ (plural, GET+DELETE
# allowed), not /upload/ (singular, POST-only by design, used only for
# submitting new files). This is a deliberate separation, not a bug.
check \
"Download uploaded file" \
"curl -i -s $URL/uploads/eval_test_upload.txt" \
"42"


# ----------------------------------------------------------
# DIRECTORY INDEX
# ----------------------------------------------------------

check \
"Directory request" \
"curl -i -s $URL/" \
"200"


# ----------------------------------------------------------
# REDIRECT
# ----------------------------------------------------------

echo
echo "Redirect test"

# FIXED: /redirect was never a configured route. The actual redirect
# in this config is /old -> /new (301).
check \
"Redirect (/old -> /new)" \
"curl -i -s $URL/old" \
"301"


# ----------------------------------------------------------
# CGI TESTS
# ----------------------------------------------------------

echo
echo "CGI TESTS"

# FIXED: the real CGI scripts live under /cgi (e.g. hello.py), not under
# a /cgi-bin/test.py path that never existed in this project.
check \
"CGI GET" \
"curl -i -s $URL/cgi/hello.py" \
"200"


check \
"CGI POST" \
"curl -i -s -X POST -d 'hello=world' $URL/cgi/hello.py" \
"200"


# ----------------------------------------------------------
# PORT TEST
# ----------------------------------------------------------

echo
echo "Checking ports"

netstat -tln 2>/dev/null | grep $PORT

if [ $? -eq 0 ]
then
    echo -e "${GREEN}Port $PORT listening${RESET}"
else
    echo -e "${RED}Port not found${RESET}"
fi


# cleanup temp fixture created during this run
rm -f www/delete_me.txt /tmp/eval_big_body.txt /tmp/eval_test_upload.txt


# ----------------------------------------------------------
# SIEGE TESTS
# ----------------------------------------------------------

if command -v siege >/dev/null
then

echo
echo "================================="
echo "Siege stress tests"
echo "================================="


echo
echo "Simple GET benchmark"

siege \
-b \
-c 50 \
-d 1 \
-t 1M \
$URL


echo
echo "Heavy concurrency"

siege \
-c 200 \
-d 1 \
-r 20 \
$URL


echo
echo "Long running test"

siege \
-b \
-c 100 \
-d 1 \
-t 5M \
$URL


else

echo -e "${YELLOW}Siege not installed${RESET}"

fi


# ----------------------------------------------------------
# SUMMARY
# ----------------------------------------------------------

echo
echo "================================="
echo "RESULT"
echo "================================="

echo -e "${GREEN}Passed: $PASS${RESET}"
echo -e "${RED}Failed: $FAIL${RESET}"


echo
echo "Manual checks still required:"
echo "- Browser network tab"
echo "- Multiple hosts/interfaces"
echo "- Virtual hosts"
echo "- Telnet raw HTTP requests"
echo "- Two webserv processes same port"
echo "- CGI infinite loop timeout"
echo "- Valgrind memory leaks"
echo "- Directory listing behaviour"

echo
echo "Done."
