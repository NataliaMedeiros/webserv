#!/bin/bash

# ==========================================================
# Webserv Siege Stress Tester
# 42 School Webserv project
# ==========================================================

HOST="http://localhost:8080"

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
RESET="\033[0m"

echo "=============================================="
echo "        Webserv Siege Stress Tester"
echo "=============================================="
echo

# Check siege installation
if ! command -v siege &> /dev/null
then
    echo -e "${RED}Siege is not installed.${RESET}"
    echo "Install it with:"
    echo "  sudo apt install siege"
    exit 1
fi

echo -e "${GREEN}✓ Siege found${RESET}"

# Check server availability
echo
echo "Checking server: $HOST"

if curl -s --head "$HOST" > /dev/null
then
    echo -e "${GREEN}✓ Server is responding${RESET}"
else
    echo -e "${RED}✗ Server is not reachable${RESET}"
    echo "Start your webserv first."
    exit 1
fi


run_test()
{
    NAME=$1
    OPTIONS=$2

    echo
    echo "=============================================="
    echo "$NAME"
    echo "siege $OPTIONS $HOST"
    echo "=============================================="

    siege $OPTIONS "$HOST"

    echo
    echo -e "${GREEN}Finished: $NAME${RESET}"
}


# ----------------------------------------------------------
# Test 1: Basic load
# ----------------------------------------------------------

run_test \
"Basic request test (50 users, 30 seconds)" \
"-c 50 -t 30S"


# ----------------------------------------------------------
# Test 2: Medium concurrency
# ----------------------------------------------------------

run_test \
"Medium stress test (100 users, 1 minute)" \
"-c 100 -t 1M"


# ----------------------------------------------------------
# Test 3: Heavy concurrency
# ----------------------------------------------------------

run_test \
"Heavy stress test (500 users, 2 minutes)" \
"-c 500 -t 2M"


# ----------------------------------------------------------
# Test 4: Repeated requests
# ----------------------------------------------------------

run_test \
"Repeated requests test (200 users x 20 requests)" \
"-c 200 -r 20"


# ----------------------------------------------------------
# Test 5: Keep alive stress
# ----------------------------------------------------------

echo
echo "=============================================="
echo "Keep-alive stress test"
echo "=============================================="

siege \
-c 100 \
-t 1M \
-H "Connection: keep-alive" \
"$HOST"


echo
echo "=============================================="
echo -e "${GREEN}All Siege tests completed${RESET}"
echo "=============================================="