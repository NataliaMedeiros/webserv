#!/bin/bash
# Run this once to create the test folders and files
mkdir -p www/images
mkdir -p www/files
mkdir -p www/uploads
echo "<html><body><h1>Home</h1></body></html>" > www/index.html
echo "<html><body><h1>About</h1></body></html>" > www/about.html
echo "(fake jpeg bytes)" > www/images/cat.jpg
echo "file one" > www/files/one.txt
echo "file two" > www/files/two.txt
echo "Setup done."
