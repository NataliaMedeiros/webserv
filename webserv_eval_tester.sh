#!/bin/bash

# ==========================================================
# 42 Webserv Evaluation Tester
# Automated checks + Siege stress tests
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
echo "       42 Webserv Evaluation Tester"
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


check \
"DELETE request" \
"curl -i -s -X DELETE $URL/test.txt" \
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

BIG_BODY=$(python3 - <<EOF
print("A"*1000000)
EOF
)

RESULT=$(curl -i -s \
-X POST \
-H "Content-Type: plain/text" \
--data "$BIG_BODY" \
$URL)

if echo "$RESULT" | grep -q "413"
then
    echo -e "${GREEN}PASS - Body limit enforced${RESET}"
else
    echo -e "${YELLOW}Check manually - depends on configuration${RESET}"
fi



# ----------------------------------------------------------
# FILE UPLOAD / DOWNLOAD
# ----------------------------------------------------------

echo "Upload test file"

echo "42 Webserv upload test" > test_upload.txt


check \
"Upload file" \
"curl -i -s -X POST --data-binary @test_upload.txt $URL/upload" \
"200"


check \
"Download uploaded file" \
"curl -i -s $URL/upload/test_upload.txt" \
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

curl -i -s $URL/redirect | grep Location

if [ $? -eq 0 ]
then
    echo -e "${GREEN}Redirect detected${RESET}"
else
    echo -e "${YELLOW}No redirect found${RESET}"
fi



# ----------------------------------------------------------
# CGI TESTS
# ----------------------------------------------------------

echo
echo "CGI TESTS"

check \
"CGI GET" \
"curl -i -s $URL/cgi-bin/test.py" \
"200"


check \
"CGI POST" \
"curl -i -s -X POST -d 'hello=world' $URL/cgi-bin/test.py" \
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



