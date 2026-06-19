#include "HttpRequest.hpp"
#include "HttpRequestParser.hpp"
#include <iostream>
#include <cassert>
#include <string>

// ============================================================
//  Helper — print a labelled result without crashing on
//  NeedMore / BadRequest (req fields are meaningless then).
// ============================================================
static std::string resultName(HttpRequestParser::Result r)
{
	if (r == HttpRequestParser::NeedMore)   return "NeedMore";
	if (r == HttpRequestParser::Complete)   return "Complete";
	return "BadRequest";
}

static void print(const std::string& label,
                  HttpRequestParser::Result res,
                  const HttpRequest& req)
{
	std::cout << "--- " << label << "\n";
	std::cout << "    result  : " << resultName(res) << "\n";
	if (res == HttpRequestParser::Complete)
	{
		std::cout << "    method  : " << req.method  << "\n";
		std::cout << "    path    : " << req.path    << "\n";
		std::cout << "    query   : " << req.query_string << "\n";
		std::cout << "    version : " << req.version << "\n";
		std::cout << "    headers :\n";
		for (std::map<std::string,std::string>::const_iterator it =
		         req.headers.begin(); it != req.headers.end(); ++it)
			std::cout << "      " << it->first << ": " << it->second << "\n";
		if (!req.query_params.empty())
		{
			std::cout << "    params  :\n";
			for (std::map<std::string,std::string>::const_iterator it =
			         req.query_params.begin(); it != req.query_params.end(); ++it)
				std::cout << "      " << it->first << " = " << it->second << "\n";
		}
		std::cout << "    body    : [" << req.body << "]\n";
	}
	std::cout << "\n";
}

// ============================================================
//  SECTION 1 — Happy path (requests that must succeed)
// ============================================================
static void section1()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 1 — Happy path\n";
	std::cout << "========================================\n\n";

	// T01 — minimal GET
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T01: minimal GET", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.method == "GET");
		assert(r.path   == "/");
		assert(r.body   == "");
	}

	// T02 — GET with deep path
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /api/v1/users/42 HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T02: GET deep path", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.path == "/api/v1/users/42");
	}

	// T03 — POST with Content-Length, single feed
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /login HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 12\r\n\r\nusername=joe", r);
		print("T03: POST single feed", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "username=joe");
	}

	// T04 — DELETE with no body
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("DELETE /api/item/5 HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T04: DELETE no body", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.method == "DELETE");
		assert(r.body   == "");
	}

	// T05 — multiple headers stored correctly
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /page HTTP/1.1\r\nHost: example.com\r\n"
			       "Accept: text/html\r\nConnection: keep-alive\r\n\r\n", r);
		print("T05: multiple headers", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.headers["accept"]     == "text/html");
		assert(r.headers["connection"] == "keep-alive");
	}

	// T06 — Content-Length: 0 on POST (valid empty body)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /ping HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 0\r\n\r\n", r);
		print("T06: POST Content-Length 0", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "");
	}
}

// ============================================================
//  SECTION 2 — Chunked transfer encoding
// ============================================================
static void section2()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 2 — Chunked transfer encoding\n";
	std::cout << "========================================\n\n";

	// T07 — chunked body in one shot
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /upload HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n", r);
		print("T07: chunked single feed", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "Hello World");
	}

	// T08 — chunked body arriving in two feeds
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result r1 =
			p.feed("POST /upload HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n5\r\nHel", r);
		assert(r1 == HttpRequestParser::NeedMore);
		HttpRequestParser::Result res = p.feed("lo\r\n0\r\n\r\n", r);
		print("T08: chunked across two feeds", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "Hello");
	}

	// T09 — chunked with chunk extension (semicolon)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /upload HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "5; name=first\r\nHello\r\n0\r\n\r\n", r);
		print("T09: chunked with extension", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "Hello");
	}

	// T10 — Transfer-Encoding takes priority over Content-Length
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /upload HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 999\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "5\r\nHello\r\n0\r\n\r\n", r);
		print("T10: TE chunked wins over CL", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "Hello");
	}
}

// ============================================================
//  SECTION 3 — Query string and percent-encoding
// ============================================================
static void section3()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 3 — Query string & percent-encoding\n";
	std::cout << "========================================\n\n";

	// T11 — basic query string
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /search?q=hello&page=2 HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T11: basic query string", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.path         == "/search");
		assert(r.query_string == "q=hello&page=2");
		assert(r.query_params["q"]    == "hello");
		assert(r.query_params["page"] == "2");
	}

	// T12 — plus sign decoded as space
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /search?q=hello+world HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T12: + decoded as space", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.query_params["q"] == "hello world");
	}

	// T13 — percent-encoded characters
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /p?name=John%20Doe&code=%2B1 HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T13: percent-encoded query", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.query_params["name"] == "John Doe");
		assert(r.query_params["code"] == "+1");
	}

	// T14 — key with no value
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /page?debug HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T14: key with no value", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.query_params["debug"] == "");
	}

	// T15 — no query string
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /plain HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T15: no query string", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.path         == "/plain");
		assert(r.query_string == "");
		assert(r.query_params.empty());
	}
}

// ============================================================
//  SECTION 4 — NeedMore (incomplete requests)
// ============================================================
static void section4()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 4 — NeedMore\n";
	std::cout << "========================================\n\n";

	// T16 — headers not yet complete (no \r\n\r\n)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\nHost: example.com", r);
		print("T16: headers incomplete", res, r);
		assert(res == HttpRequestParser::NeedMore);
	}

	// T17 — headers complete, body partially arrived
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /upload HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 10\r\n\r\nhel", r);
		print("T17: partial body", res, r);
		assert(res == HttpRequestParser::NeedMore);
	}

	// T18 — only method arrived so far
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res = p.feed("GET", r);
		print("T18: only method", res, r);
		assert(res == HttpRequestParser::NeedMore);
	}

	// T19 — req is untouched on NeedMore
	{
		HttpRequestParser p; HttpRequest r;
		r.method = "SENTINEL";
		p.feed("POST /up HTTP/1.1\r\nContent-Length: 10\r\n\r\nhel", r);
		std::cout << "--- T19: req untouched on NeedMore\n";
		std::cout << "    result  : NeedMore\n";
		std::cout << "    req.method still SENTINEL : "
		          << (r.method == "SENTINEL" ? "PASS" : "FAIL") << "\n\n";
		assert(r.method == "SENTINEL");
	}

	// T20 — chunked body incomplete (chunk data cut off)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /up HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "5\r\nHel", r);
		print("T20: chunked body cut off", res, r);
		assert(res == HttpRequestParser::NeedMore);
	}

	// T21 — chunked terminator not yet arrived
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /up HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "5\r\nHello\r\n", r);
		print("T21: chunked terminator missing", res, r);
		assert(res == HttpRequestParser::NeedMore);
	}
}

// ============================================================
//  SECTION 5 — BadRequest (malformed or rejected requests)
// ============================================================
static void section5()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 5 — BadRequest\n";
	std::cout << "========================================\n\n";

	// T22 — only one token in request line
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res = p.feed("GET\r\n\r\n", r);
		print("T22: only one token", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T23 — only two tokens (missing version)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET /index.html\r\n\r\n", r);
		print("T23: missing version", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T24 — unsupported method
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("PUT /x HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T24: PUT -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T25 — unsupported HTTP version
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n", r);
		print("T25: HTTP/1.0 -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T26 — empty path (two spaces between method and version)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET  HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T26: empty path -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T27 — absolute URI (proxy-style), path does not start with /
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET http://example.com/path HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T27: absolute URI -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T28 — missing Host header
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\n\r\n", r);
		print("T28: missing Host -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T29 — POST without Content-Length or Transfer-Encoding
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /submit HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T29: POST no CL no TE -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T30 — Content-Length: abc (non-numeric)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: abc\r\n\r\nhello", r);
		print("T30: CL non-numeric -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T31 — Content-Length: -5 (negative)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: -5\r\n\r\nhello", r);
		print("T31: CL negative -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T32 — Content-Length: 12abc (mixed)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 12abc\r\n\r\nhello", r);
		print("T32: CL mixed -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T33 — Content-Length exceeds MAX_BUFFER_SIZE
	{
		HttpRequestParser p; HttpRequest r;
		// 9 * 1024 * 1024 = 9 MB > 8 MB ceiling
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 9437184\r\n\r\n", r);
		print("T33: CL exceeds max -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T34 — duplicate Content-Length
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 5\r\n"
			       "Content-Length: 999\r\n\r\nhello", r);
		print("T34: duplicate CL -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T35 — buffer overflow (endless headers, no \r\n\r\n)
	{
		HttpRequestParser p; HttpRequest r;
		std::string giant = "GET / HTTP/1.1\r\nX-Junk: ";
		giant += std::string(HttpRequestParser::MAX_BUFFER_SIZE, 'A');
		HttpRequestParser::Result res = p.feed(giant, r);
		print("T35: buffer overflow -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T36 — invalid chunk size (non-hex characters)
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /up HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "ZZ\r\ndata\r\n0\r\n\r\n", r);
		print("T36: invalid chunk size -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T37 — chunk data missing trailing CRLF
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /up HTTP/1.1\r\nHost: example.com\r\n"
			       "Transfer-Encoding: chunked\r\n\r\n"
			       "5\r\nHelloXX0\r\n\r\n", r);
		print("T37: chunk missing CRLF -> BadRequest", res, r);
		assert(res == HttpRequestParser::BadRequest);
	}

	// T38 — header line with empty name (": value") is silently skipped,
	//        but the request itself is still valid
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\nHost: example.com\r\n"
			       ": ignored\r\n\r\n", r);
		print("T38: empty header name silently skipped", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.headers.count("") == 0); // empty key must NOT be stored
	}
}

// ============================================================
//  SECTION 6 — Header edge cases
// ============================================================
static void section6()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 6 — Header edge cases\n";
	std::cout << "========================================\n\n";

	// T39 — mixed-case header name normalised to lowercase
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "CoNtEnT-LeNgTh: 5\r\n\r\nhello", r);
		print("T39: mixed-case header normalised", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.headers.count("content-length") == 1);
		assert(r.body == "hello");
	}

	// T40 — trailing whitespace on header value trimmed
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("POST /x HTTP/1.1\r\nHost: example.com\r\n"
			       "Content-Length: 5   \r\n\r\nhello", r);
		print("T40: trailing whitespace on CL trimmed", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "hello");
	}

	// T41 — leading whitespace on header value trimmed
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\nHost:   example.com\r\n\r\n", r);
		print("T41: leading whitespace on value trimmed", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.headers["host"] == "example.com");
	}

	// T42 — header line without colon is silently skipped
	{
		HttpRequestParser p; HttpRequest r;
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\nHost: example.com\r\n"
			       "BadHeaderLine\r\n\r\n", r);
		print("T42: header without colon skipped", res, r);
		assert(res == HttpRequestParser::Complete);
	}
}

// ============================================================
//  SECTION 7 — Multi-feed and pipelining
// ============================================================
static void section7()
{
	std::cout << "========================================\n";
	std::cout << " SECTION 7 — Multi-feed and pipelining\n";
	std::cout << "========================================\n\n";

	// T43 — POST arriving across three feeds
	{
		HttpRequestParser p; HttpRequest r;
		assert(p.feed("POST /login HTTP/1.1\r\nHost: example.com\r\nCont", r)
		       == HttpRequestParser::NeedMore);
		assert(p.feed("ent-Length: 12\r\n\r\nuser", r)
		       == HttpRequestParser::NeedMore);
		HttpRequestParser::Result res = p.feed("name=joe", r);
		print("T43: POST across three feeds", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.body == "username=joe");
	}

	// T44 — two complete requests concatenated in one buffer (pipelining)
	{
		HttpRequestParser p; HttpRequest r;
		std::string two =
			"GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n"
			"GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";
		HttpRequestParser::Result res1 = p.feed(two, r);
		print("T44a: pipelining — first request", res1, r);
		assert(res1 == HttpRequestParser::Complete);
		assert(r.path == "/first");

		// Feed an empty string — parser should now return the second request
		// that was left in _buf from the previous call
		HttpRequest r2;
		HttpRequestParser::Result res2 = p.feed("", r2);
		print("T44b: pipelining — second request", res2, r2);
		assert(res2 == HttpRequestParser::Complete);
		assert(r2.path == "/second");
	}

	// T45 — parser recovers cleanly after BadRequest
	{
		HttpRequestParser p; HttpRequest r;
		// First call: bad
		p.feed("PUT /x HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		// Second call: valid — must not see leftover bytes from the bad one
		HttpRequestParser::Result res =
			p.feed("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", r);
		print("T45: recovers after BadRequest", res, r);
		assert(res == HttpRequestParser::Complete);
		assert(r.method == "GET");
	}

	// T46 — trailing bytes after body left for next request
	{
		HttpRequestParser p; HttpRequest r;
		// First request has body "hello" (5 bytes), followed by start of second
		std::string two =
			"POST /a HTTP/1.1\r\nHost: example.com\r\n"
			"Content-Length: 5\r\n\r\n"
			"helloGET /b HTTP/1.1\r\nHost: example.com\r\n\r\n";
		HttpRequestParser::Result res1 = p.feed(two, r);
		print("T46a: trailing bytes — first request", res1, r);
		assert(res1 == HttpRequestParser::Complete);
		assert(r.body == "hello");

		HttpRequest r2;
		HttpRequestParser::Result res2 = p.feed("", r2);
		print("T46b: trailing bytes — second request", res2, r2);
		assert(res2 == HttpRequestParser::Complete);
		assert(r2.path == "/b");
	}
}

// ============================================================
//  main
// ============================================================
int main()
{
	section1();
	section2();
	section3();
	section4();
	section5();
	section6();
	section7();

	std::cout << "========================================\n";
	std::cout << " ALL 46 TESTS PASSED\n";
	std::cout << "========================================\n";
	return 0;
}
