#include "Router.hpp"
#include "Handler.hpp"
#include "FileSystem.hpp"
#include "Http.hpp"
#include <iostream>

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

    RouteDecision rd = router.route(req);
//     std::cout << "  Matched root   : " << rd.root << "\n";
//     std::cout << "  Matched index  : " << rd.index << "\n";
//     std::cout << "  Redirect code  : " << rd.redirectCode << "\n";
//     std::cout << "  Autoindex      : " << (rd.autoindex ? "yes" : "no") << "\n";

    HttpResponse res = handler.handle(rd, req);
    printResponse(res);
}

int main()
{
    // ── Build fake config ──────────────────────────────────────────
    ServerConfig config;
    config.root  = "./www";
    config.index = "index.html";

    LocationConfig locRoot;
    locRoot.path      = "/";
    locRoot.root      = "./www";
    locRoot.index     = "index.html";
    locRoot.autoindex = false;
    locRoot.methods   = {"GET", "POST", "DELETE"};

    LocationConfig locImages;
    locImages.path      = "/images";
    locImages.root      = "./www/images";
    locImages.autoindex = false;
    locImages.methods   = {"GET"};

    LocationConfig locUpload;
    locUpload.path       = "/upload";
    locUpload.root       = "./www";
    locUpload.uploadPath = "./www/uploads";
    locUpload.methods    = {"POST"};

    LocationConfig locRedirect;
    locRedirect.path        = "/old";
    locRedirect.redirectCode = 301;
    locRedirect.redirectUrl  = "/new";

    LocationConfig locAuto;
    locAuto.path      = "/files";
    locAuto.root      = "./www/files";
    locAuto.autoindex = true;
    locAuto.methods   = {"GET"};

    config.locations.push_back(locRoot);
    config.locations.push_back(locImages);
    config.locations.push_back(locUpload);
    config.locations.push_back(locRedirect);
    config.locations.push_back(locAuto);

    Router  router(config);
    std::cout << "\n========== ROUTER TESTS ==========\n";

   // Helper
   auto testRoute = [&](const std::string& path)
   {
                HttpRequest req;
                req.method = "GET";
                req.path = path;

        RouteDecision rd = router.route(req);
        //print
        std::cout << "\nRequest path: " << path << "\n";
        std::cout << "Matched root : " << rd.root << "\n";
        std::cout << "Matched index: " << rd.index << "\n";
        std::cout << "RedirectCode : " << rd.redirectCode << "\n";
   };
   std::cout << "\n--- Test 1: Longest Prefix ---\n";
   testRoute("/images/cat.jpg");//Expexted Matched root : ./www/images, NOT ./www
   std::cout << "\n--- Test 2: Fallback ---\n";
   testRoute("/about.html");//Expected Matched root : ./www, since /about.html does not match /images
   std::cout << "\n--- Test 3: Boundary ---\n";
   testRoute("/imageset/file.jpg");//Expected Matched root : ./www, ❌ MUST NOT be ./www/images
   std::cout << "\n--- Test 4: Exact match ---\n";
   testRoute("/images");//Expected Matched root : ./www/images, since /images is an exact match for the /images location, even though /images/cat.jpg would also match
   std::cout << "\n--- Test 5: Root ---\n";
   testRoute("/");//Expected Matched root : ./www since / matches everything but is the longest match for the root path
   std::cout << "\n--- Test 6: Redirect ---\n";
   testRoute("/old");//Expected Matched root : ./www, RedirectCode : 301 since /old matches the /old location which has a redirect configured

   std::cout << "\n========== FILESYSTEM TESTS ==========\n";
        
   std::cout << "\n--- TEST 1: exists() ---\n";
   std::cout << "./www/index.html : "
        << (FileSystem::exists("./www/index.html") ? "YES" : "NO") << "\n";//expected:yes
   std::cout << "./www/NOFILE.txt : "
        << (FileSystem::exists("./www/NOFILE.txt") ? "YES" : "NO") << "\n";//expected:no

   std::cout << "\n--- TEST 2: isFileNormal() / isDir() ---\n";
   std::cout << "index.html isFile: "
        << (FileSystem::isFileNormal("./www/index.html") ? "YES" : "NO") << "\n";//expected:yes
   std::cout << "images isDir: "
        << (FileSystem::isDir("./www/images") ? "YES" : "NO") << "\n";//expected:yes
   std::cout << "images isDir: "
        << (FileSystem::isDir("./images") ? "YES" : "NO") << "\n";//expected:no

   std::cout << "\n--- TEST 3: readFile ---\n";
   std::string content;
   if (FileSystem::readFile("./www/index.html", content))//expected:Read OK, size: (some number > 0)
        std::cout << "Read OK, size: " << content.size() << "\n";
   else
        std::cout << "Read FAILED\n";

   std::cout << "\n--- TEST 4:listDir ---\n";
   std::vector<std::string> files;
   if (FileSystem::listDir("./www/files", files))
   {
        for (size_t i = 0; i < files.size(); i++)
        std::cout << files[i] << "\n";
   }
   else
        std::cout << "listDir FAILED\n";
        
   std::cout << "\n--- TEST 5: writeFile ---\n";
   if (FileSystem::writeFile("./www/uploads/test2.txt", "hello world"))//change text and check folder
        std::cout << "Write OK\n";
   else
        std::cout << "Write FAILED\n";
        
   std::cout << "\n--- TEST 6:mimeType ---\n";
   std::cout << "index.html: "
        << FileSystem::mimeType("index.html") << "\n";//expected:text/html
   std::cout << "cat.jpg: "
        << FileSystem::mimeType("cat.jpg") << "\n";//expected:image/jpeg
   std::cout << "file.unknown: "
        << FileSystem::mimeType("file.abc") << "\n";//expected:application/octet-stream
    
   //make handler object
   Handler handler;

   std::cout << "══════════════════════════════════════════════\n";
   std::cout << "  ROUTER + HANDLER TEST\n";
   std::cout << "══════════════════════════════════════════════\n";

   std::cout << "TEST 1: BASIC HANDLER ERROR TEST\n";
   runTest("GET existing file",
        router, handler, "GET", "/index.html");

   runTest("GET image",
        router, handler, "GET", "/images/cat.jpg");

   runTest("GET not found",
        router, handler, "GET", "/nope.html");

   runTest("POST should fail",
        router, handler, "POST", "/index.html");


//     // Test 1: basic longest-match
//     runTest("T1 longest match: /images beats /",
//             router, handler, "GET", "/images/cat.jpg");

//     // Test 2: fallback to /
//     runTest("T2 fallback to /",
//             router, handler, "GET", "/about.html");

//     // Test 3: boundary bug check — /imageset must NOT match /images
//     runTest("T3 boundary: /imageset must not match /images",
//             router, handler, "GET", "/imageset/x.jpg");

//     // Test 4: serve root /
//     runTest("T4 root / serves index",
//             router, handler, "GET", "/");

//     // Test 5: redirect
//     runTest("T5 redirect 301",
//             router, handler, "GET", "/old");

//     // Test 6: method not allowed (POST to /images which only allows GET)
//     runTest("T6 method not allowed",
//             router, handler, "POST", "/images/cat.jpg");

//     // Test 7: autoindex directory
//     runTest("T7 autoindex on /files/",
//             router, handler, "GET", "/files/");

//     // Test 8: upload POST
//     runTest("T8 upload POST",
//             router, handler, "POST", "/upload/test.txt", "hello upload content");

//     // Test 9: DELETE
//     runTest("T9 DELETE existing file",
//             router, handler, "DELETE", "/images/cat.jpg");

    return 0;
}
