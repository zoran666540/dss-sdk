#!/usr/bin/env bash
git_hash=$(git describe --abbrev=4 --always --tags)

{
echo "/* Do not edit this file directly, it is autogenerated during build */"
echo ""
echo ""
echo "#ifndef GITHASH_H"
echo "#define GITHASH_H"
echo "const std::string dssVersion = \"$git_hash\";"
echo "#endif // GITHASH_H"
} >> src/include_private/dss_version.h
