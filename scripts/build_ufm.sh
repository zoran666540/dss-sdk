#!/usr/bin/env bash
#
#
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..

# Read build environment vars
pushd "${script_dir}"

# shellcheck disable=SC1091
source ./build_env

release_dir="${top_dir}/ansible/release"

[[ ! -e ${release_dir} ]] && die "ERR: The Release directory does not exist: ${release_dir}"

# Clean existing RPM/DEB packages
echo "Removing existing RPM/DEB packages"
rm -f "${release_dir}/nkvagent*.deb"
rm -f "${release_dir}/nkvagent*.rpm"

# Build Fabric Manager
echo "Building Fabric Mananger"

pushd "${top_dir}/ufm/fabricmanager"
./makeufmpackage.sh -prJ "$UFM_VER"

cp ./*.rpm "${release_dir}/"
cp ./*.deb "${release_dir}/"
popd

# Build Message Broker
echo "Building Message Broker"

pushd "${top_dir}/ufm/ufm_msg_broker"
./makebrokerpackage.sh -pr -J "$UFM_BROKER_VER"

cp ./*.rpm "${release_dir}/"
cp ./*.deb "${release_dir}/"

popd
