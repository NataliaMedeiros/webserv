# How to run the (fixed) evaluation tester

## Why the original script showed 4 failures

The original `webserv_eval_tester.sh` is a generic script that assumes generic paths and files (`/test.txt`, `/upload` for downloading, `/cgi-bin/test.py`, etc.) that don't match how **this specific project** is actually configured. When re-tested against the real, configured routes, every single one of those 4 features works correctly. None were real server bugs. 

Details:
- Test 1 that Failed: DELETE
Why it failed: Targeted `/test.txt`, which never existed at the www root
Confirmed working via: `/files/a.txt` (or any real file) → `204 No Content`
- Test 2 that Failed: Upload
Why it failed: Sent a raw body (`--data-binary`), not multipart/form-data, and expected `200` instead of `201`
Confirmed working via: `-F 'file=@...'` → `201 Created`
- Test 3 that Failed: Download uploaded file
Why it failed: Targeted `/upload` (write-only, POST-only by design) instead of `/uploads` (the read-back route)
Confirmed working via: `/uploads/<file>` → `200 OK` with correct content
- Test 4 that Failed: CGI GET
Why it failed: Targeted `/cgi-bin/test.py`, which never existed
Confirmed working: `/cgi/hello.py` → `200 OK` 

The body-limit test also failed for an unrelated reason: it built a 1MB string as a shell argument, which hit the OS's own `Argument list too long` limit before curl ever ran. Fixed by writing to a temp file and using `--data-binary @file` instead.

## Setup steps

1. Use **`configs/default_with_error.config`**, this is the only config with `/upload`, `/uploads`, and `/cgi` all defined together, which is what the tester needs.

2. Build and start the server from the project root:
   ```
   make fclean
   make
   ./webserv configs/default_with_error.config
   ```

3. In a separate terminal, from the project root, run the fixed tester:
   ```
   bash webserv_eval_tester_fixed.sh
   ```

4. Expected result: `Passed: 12`, `Failed: 0` (plus the Siege stress section, if `siege` is installed).

## What changed in the script

- DELETE now targets a file created fresh at the start of the run (`www/delete_me.txt`), cleaned up afterwards.
- Body-limit test writes the large body to a temp file first, avoiding the shell argument-length crash.
- Upload test uses `-F` (real multipart/form-data) and expects `201`, not `200`.
- Download test reads from `/uploads/`, not `/upload/`.
- Redirect test checks `/old` (the real configured redirect), not `/redirect`.
- CGI tests use `/cgi/hello.py`, the real script path.

## Team decisions

- **`201 Created` for uploads: keeping it.** Decided this is the more accurate status code, a new resource was genuinely created on the server, so `201` is the correct HTTP semantics here, not just "defensible". No change needed.
- Should `/upload` (write) and `/uploads` (read) stay as two separate routes, or would a single `/upload` route handling both GET and POST feel clearer during the defense? Still open, worth a quick team check-in.
