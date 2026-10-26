#!/bin/sh
# Install apt packages on a GitHub runner: apt-install.sh <package>...
#
# The runner's Azure mirror is sometimes very slow or stalls. Each attempt
# is capped, keeps what it already downloaded, and later attempts switch to
# archive.ubuntu.com.
set -u
o="-o Acquire::Retries=3 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30"
for attempt in 1 2 3; do
  if [ "$attempt" -gt 1 ]; then
    echo "apt attempt $((attempt - 1)) failed or stalled; retrying from archive.ubuntu.com"
    for f in /etc/apt/apt-mirrors.txt /etc/apt/sources.list /etc/apt/sources.list.d/*; do
      [ -f "$f" ] && sudo sed -i 's#azure\.archive\.ubuntu\.com#archive.ubuntu.com#g' "$f"
    done
  fi
  sudo timeout -k 15s 12m sh -c "apt-get $o update && apt-get $o install -y $*" && exit 0
done
exit 1
