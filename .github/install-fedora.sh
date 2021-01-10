#!/bin/bash
#
# This is an install script for Fedora-specific packages.
#
set -ex

# Base build packages
PACKAGES=(
	git
	gcc
	compiler-rt
	libasan
	libubsan
	clang
	meson
	perl
	pkgconf-pkg-config
	which
	cmake
)

if ! grep '^fastestmirror' /etc/dnf/dnf.conf; then
	echo 'fastestmirror=1' >> /etc/dnf/dnf.conf
fi

dnf clean all
dnf update -y
dnf install -y "${PACKAGES[@]}"

exit 0
