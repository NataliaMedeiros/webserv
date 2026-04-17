# Person 3: Router + Handler + FileSystem

Standalone, tested. **Not yet integrated with main.**

## What works
- Router with longest-prefix matching and boundary check
- Static file serving (GET)
- DELETE method
- Redirects (301/302/307/308)
- Method restriction per route
- Error responses (404, 405, 500)
- Alias-style path resolution (subject's kapouet example)

## Not yet implemented
- ConfigParser (config hardcoded in `main.cpp` for now)
- Upload (multipart parsing)
- CGI
- Default error pages from config
- Autoindex (written, not tested end-to-end)

## Integration notes for later
My `RouteDecision` has extra fields Person 1's placeholder doesn't:
`index`, `autoindex`, `locationPath`, `redirectCode`, `redirectUrl`,
`methods`, `uploadPath`, `cgiPass`.

My Handler is a class (`Handler::handle(rd, req)`), not free functions
(`Handlers::serveStatic`). To decide at team meeting.
