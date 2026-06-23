#include "Router.hpp"
#include "Handler.hpp"
#include "FileSystem.hpp"
#include "Http.hpp"
#include <iostream>
#include "ConfigParser.hpp"

//Helper: print an HttpResponse summary
static void printResponse(const HttpResponse& res)
{
    std::cout << "  Status : " << res.status << " " << res.reason << "\n";
//     for (const auto& h : res.headers)
//         std::cout << "  Header : " << h.first << ": " << h.second << "\n";
    if (!res.body.empty() && res.body.size() < 200)
        std::cout << "  Body   : " << res.body << "\n";
    else if (!res.body.empty())
        std::cout << "  Body   : (" << res.body.size() << " bytes)\n";
}

static void runTest(const std::string& label,
                    Router& router, Handler& handler,
                    const std::string& method, const std::string& path,
                    const std::string& body = "")
{
    std::cout << "\n── " << label << "\n";
    std::cout << "  Request: " << method << " " << path << "\n";

    HttpRequest req;
    req.method = method;
    req.path   = path;
    req.body   = body;
    req.headers["Content-Type"] =
    "multipart/form-data; boundary=----test123";

    RouteDecision rd = router.route(req);
    std::cout << "  Matched root   : " << rd.root << "\n";
//     std::cout << "  Matched index  : " << rd.index << "\n";
//     std::cout << "  Redirect code  : " << rd.redirectCode << "\n";
//     std::cout << "  Autoindex      : " << (rd.autoindex ? "yes" : "no") << "\n";

    HttpResponse res = handler.handle(rd, req);
    printResponse(res);
}

int main(int argc, char* argv[])
{

     if (argc != 2) 
     {
          std::cerr << "Usage: " << argv[0] << " <config_file>\n";
          return 1;
     }
     ServerConfig config; 
     try 
     {
          config = ConfigParser::parse(argv[1]);
     } 
     catch (const std::exception& e) 
     {
          std::cerr << "Config error: " << e.what() << "\n";
          return 1;
     }

     //     // ── Build fake config ──────────────────────────────────────────
//     ServerConfig config;
//     config.root  = "./www";
//     config.index = "index.html";

//     LocationConfig locRoot;
//     locRoot.path      = "/";
//     locRoot.root      = "./www";
//     locRoot.index     = "index.html";
//     locRoot.autoindex = false;
//     locRoot.methods   = {"GET", "POST", "DELETE"};

//     LocationConfig locImages;
//     locImages.path      = "/images";
//     locImages.root      = "./www/images";
//     locImages.autoindex = false;
//     locImages.methods   = {"GET"};

//     LocationConfig locUpload;
//     locUpload.path       = "/upload";
//     locUpload.root       = "./www";
//     locUpload.uploadPath = "./www/uploads";
//     locUpload.methods    = {"POST"};

//     LocationConfig locRedirect;
//     locRedirect.path        = "/old";
//     locRedirect.redirectCode = 301;
//     locRedirect.redirectUrl  = "/new";

//     LocationConfig locAuto;
//     locAuto.path      = "/files";
//     locAuto.root      = "./www/files";
//     locAuto.autoindex = true;
//     locAuto.methods   = {"GET"};

//     config.locations.push_back(locRoot);
//     config.locations.push_back(locImages);
//     config.locations.push_back(locUpload);
//     config.locations.push_back(locRedirect);
//     config.locations.push_back(locAuto);

     Router  router(config);
//     std::cout << "\n========== ROUTER TESTS ==========\n";

//    // Helper
//    auto testRoute = [&](const std::string& path)
//    {
//                 HttpRequest req;
//                 req.method = "GET";
//                 req.path = path;

//         RouteDecision rd = router.route(req);
//         //print
//         std::cout << "\nRequest path: " << path << "\n";
//         std::cout << "Matched root : " << rd.root << "\n";
//         std::cout << "Matched index: " << rd.index << "\n";
//         std::cout << "RedirectCode : " << rd.redirectCode << "\n";
//    };
//    std::cout << "\n--- Test 1: Longest Prefix ---\n";
//    testRoute("/images/cat.jpg");//Expexted Matched root : ./www/images, NOT ./www
//    std::cout << "\n--- Test 2: Fallback ---\n";
//    testRoute("/about.html");//Expected Matched root : ./www, since /about.html does not match /images
//    std::cout << "\n--- Test 3: Boundary ---\n";
//    testRoute("/imageset/file.jpg");//Expected Matched root : ./www, ❌ MUST NOT be ./www/images
//    std::cout << "\n--- Test 4: Exact match ---\n";
//    testRoute("/images");//Expected Matched root : ./www/images, since /images is an exact match for the /images location, even though /images/cat.jpg would also match
//    std::cout << "\n--- Test 5: Root ---\n";
//    testRoute("/");//Expected Matched root : ./www since / matches everything but is the longest match for the root path
//    std::cout << "\n--- Test 6: Redirect ---\n";
//    testRoute("/old");//Expected Matched root : ./www, RedirectCode : 301 since /old matches the /old location which has a redirect configured

//    std::cout << "\n========== FILESYSTEM TESTS ==========\n";
        
//    std::cout << "\n--- TEST 1: exists() ---\n";
//    std::cout << "./www/index.html : "
//         << (FileSystem::exists("./www/index.html") ? "YES" : "NO") << "\n";//expected:yes
//    std::cout << "./www/NOFILE.txt : "
//         << (FileSystem::exists("./www/NOFILE.txt") ? "YES" : "NO") << "\n";//expected:no

//    std::cout << "\n--- TEST 2: isFileNormal() / isDir() ---\n";
//    std::cout << "index.html isFile: "
//         << (FileSystem::isFileNormal("./www/index.html") ? "YES" : "NO") << "\n";//expected:yes
//    std::cout << "images isDir: "
//         << (FileSystem::isDir("./www/images") ? "YES" : "NO") << "\n";//expected:yes
//    std::cout << "images isDir: "
//         << (FileSystem::isDir("./images") ? "YES" : "NO") << "\n";//expected:no

//    std::cout << "\n--- TEST 3: readFile ---\n";
//    std::string content;
//    if (FileSystem::readFile("./www/index.html", content))//expected:Read OK, size: (some number > 0)
//         std::cout << "Read OK, size: " << content.size() << "\n";
//    else
//         std::cout << "Read FAILED\n";

//    std::cout << "\n--- TEST 4:listDir ---\n";
//    std::vector<std::string> files;
//    if (FileSystem::listDir("./www/files", files))
//    {
//         for (size_t i = 0; i < files.size(); i++)
//         std::cout << files[i] << "\n";
//    }
//    else
//         std::cout << "listDir FAILED\n";
        
//    std::cout << "\n--- TEST 5: writeFile ---\n";
//    if (FileSystem::writeFile("./www/uploads/test2.txt", "hello world"))//change text and check folder
//         std::cout << "Write OK\n";
//    else
//         std::cout << "Write FAILED\n";
        
//    std::cout << "\n--- TEST 6:mimeType ---\n";
//    std::cout << "index.html: "
//         << FileSystem::mimeType("index.html") << "\n";//expected:text/html
//    std::cout << "cat.jpg: "
//         << FileSystem::mimeType("cat.jpg") << "\n";//expected:image/jpeg
//    std::cout << "file.unknown: "
//         << FileSystem::mimeType("file.abc") << "\n";//expected:application/octet-stream
    
   
Handler handler;

std::cout << "══════════════════════════════════════════════\n";
std::cout << "  ROUTER + HANDLER CORE TESTS\n";
std::cout << "══════════════════════════════════════════════\n";

// -------------------- BASIC --------------------

runTest("GET existing file",
        router, handler, "GET", "/index.html"); // 200

runTest("GET image",
        router, handler, "GET", "/images/cat.jpg"); // 200

runTest("GET not found",
        router, handler, "GET", "/nope.html"); // 404 (custom error page)

runTest("POST not allowed",
        router, handler, "POST", "/index.html"); // 405(custom error page)

// -------------------- ROUTING --------------------

runTest("Longest match (/images)",
        router, handler, "GET", "/images/cat.jpg"); // 200

runTest("Fallback to /",
        router, handler, "GET", "/about.html"); // 200

runTest("Boundary check (/imageset != /images)",
        router, handler, "GET", "/imageset/x.jpg"); // 404

runTest("Root index",
        router, handler, "GET", "/"); // 200

runTest("Redirect test",
        router, handler, "GET", "/old"); // 301

// -------------------- METHODS --------------------

runTest("Method not allowed (POST /images)",
        router, handler, "POST", "/images/cat.jpg"); // 405

// -------------------- SAFE DELETE TEST --------------------
//
// IMPORTANT SETUP:
//First test as it is to see no file behavior,
// then Create a safe test file:
//
//   mkdir -p www/test
//   echo "delete me" > www/test/delete.txt
//
// NEVER delete images or index.html during testing.


runTest("DELETE safe file",
        router, handler, "DELETE", "/test/delete.txt"); // 200 + file removed

runTest("VERIFY DELETE result (should be 404)",
        router, handler, "GET", "/test/delete.txt");
// EXPECT: 404 Not Found (file no longer exists)

std::cout << "\n══════════════════════════════════════════════\n";
std::cout << "  ERROR PAGE TESTS (NO CONFIG OVERRIDE)\n";
std::cout << "══════════════════════════════════════════════\n";
//test it with default config both with and without error_page configured
// 1. default_with_no_error.conf has no error_page configured
// 2. default_with_error.conf has error_page configured for 404, 405, 403
// Each test should be run twice, once with each config file, to see the difference in behavior.
//with default_with_no_error.conf, the server should return generic error pages for 404, 405, and 403.
//with default_with_error.conf, the server should return custom error pages for 404, 405, and 403.
runTest("404 fallback (no error_page)",
        router, handler, "GET", "/nope.html");


runTest("404 custom error_page",
        router, handler, "GET", "/custom/missing.html");


runTest("404 broken error_page path",
        router, handler, "GET", "/broken/missing.html");


runTest("405 method not allowed (custom error_page)",
        router, handler, "POST", "/images/cat.jpg");

std::cout << "\n══════════════════════════════════════════════\n";
std::cout << "  UPLOAD TESTS\n";
std::cout << "══════════════════════════════════════════════\n";

runTest("UPLOAD file (multipart)",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"testing.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello from upload test\n"
        "------test123--\r\n");
//expect:  Request: POST /upload
  //Status : 201 Created

runTest("UPLOAD - multiple files (should handle only first or fail cleanly)",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"a.txt\"\r\n"
        "\r\n"
        "AAA\r\n"
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file2\"; filename=\"b.txt\"\r\n"
        "\r\n"
        "BBB\r\n"
        "------test123--\r\n");

// EXPECT:
// - either only a.txt is created
// - or only first file is parsed
// - b.txt should NOT reliably exist

runTest("UPLOAD - missing filename",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"\r\n"
        "\r\n"
        "Hello\r\n"
        "------test123--\r\n");

// EXPECT:
// 400 Bad Request
// reason: No filename provided
runTest("UPLOAD - path traversal attempt",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"../../evil.txt\"\r\n"
        "\r\n"
        "HACKED\r\n"
        "------test123--\r\n");

// EXPECT:
// 400 OR rejected upload
// MUST NOT create file outside ./www/uploads
// (NO ../../ allowed)

HttpRequest req;
req.method = "POST";
req.path = "/upload";
req.headers["Content-Type"] = "multipart/form-data"; // no boundary
req.body = "randomdata";

runTest("UPLOAD - missing boundary",
        router,
        handler,
        req.method,
        req.path,
        req.body);

// EXPECT:
// 400 Malformed multipart body

req.method = "POST";
req.path = "/upload";
req.headers["Content-Type"] = "text/plain";
req.body = "hello";

runTest("UPLOAD - wrong content-type",
        router,
        handler,
        req.method,
        req.path,
        req.body);

// EXPECT:
// 400 Malformed multipart body

std::string bigContent(50000, 'A');

runTest("UPLOAD - large file",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"big.txt\"\r\n"
        "\r\n"
        + bigContent +
        "\r\n------test123--\r\n");

// EXPECT:
// 201 Created
// file created successfully
// no crash / no truncation
runTest("UPLOAD - empty filename",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"\"\r\n"
        "\r\n"
        "data\r\n"
        "------test123--\r\n");

// EXPECT:
// 400 No filename provided
runTest("UPLOAD - normal but suspicious filename",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"file..txt\"\r\n"
        "\r\n"
        "data\r\n"
        "------test123--\r\n");

// EXPECT:
// ideally reject OR sanitize filename
// SHOULD NOT break server
runTest("UPLOAD - overwrite existing file",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "\r\n"
        "FIRST VERSION\r\n"
        "------test123--\r\n");

// run again with same filename

runTest("UPLOAD - overwrite same file again",
        router,
        handler,
        "POST",
        "/upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "\r\n"
        "SECOND VERSION\r\n"
        "------test123--\r\n");
// EXPECT:
// - file is overwritten with new content
runTest("UPLOAD - GET should NOT trigger upload",
        router,
        handler,
        "GET",
        "/upload");
// EXPECT:
// - 405 Method Not Allowed
runTest("UPLOAD - no upload_path configured",
        router,
        handler,
        "POST",
        "/somewhere-without-upload",
        "------test123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "\r\n"
        "DATA\r\n"
        "------test123--\r\n");
// EXPECT:
// - 405 Method Not Allowed OR 400 No upload path configured
runTest("UPLOAD - malformed multipart body",
        router,
        handler,
        "POST",
        "/upload",
        "this is not multipart at all");
// EXPECT:
// - 400 Malformed multipart body
    return 0;
}
