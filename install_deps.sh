#!/usr/bin/env bash

set -e

echo "Creating directory structure..."
mkdir -p lib/cJSON
mkdir -p lib/tree_sitter

#######################################
# cJSON
#######################################
echo "Cloning and installing cJSON..."
git clone https://github.com/DaveGamble/cJSON tmp_cjson

cp tmp_cjson/cJSON.h lib/cJSON/
cp tmp_cjson/cJSON.c lib/cJSON/

rm -rf tmp_cjson
echo "cJSON installed."

#######################################
# tree-sitter runtime
#######################################
echo "Cloning and building tree-sitter runtime..."
git clone https://github.com/tree-sitter/tree-sitter tmp_treesitter

cd tmp_treesitter
make
cd ..

cp tmp_treesitter/libtree-sitter.a lib/tree_sitter/

# cp tmp_treesitter/lib/include/tree_sitter/parser.h lib/tree_sitter/
cp tmp_treesitter/lib/include/tree_sitter/api.h lib/tree_sitter/

rm -rf tmp_treesitter
echo "tree-sitter runtime installed."

#######################################
# tree-sitter-java grammar
#######################################
echo "Cloning tree-sitter-java for parser.c..."
git clone https://github.com/tree-sitter/tree-sitter-java tmp_ts_java

cp tmp_ts_java/src/tree_sitter/parser.h lib/tree_sitter/
cp tmp_ts_java/src/parser.c lib/tree_sitter/

rm -rf tmp_ts_java
echo "Java grammar parser installed."

echo "All dependencies installed successfully."

