#!/usr/bin/env bash

set -euo pipefail

site_directory="${1:-site}"
if [[ ! -f "${site_directory}/index.html" ||
      ! -f "${site_directory}/index.js" ||
      ! -f "${site_directory}/index.wasm" ||
      ! -f "${site_directory}/index.data" ]]; then
  echo "Web smoke test requires index.html, index.js, index.wasm, and index.data in ${site_directory}." >&2
  exit 1
fi

chrome_binary=""
for candidate in google-chrome google-chrome-stable chromium chromium-browser; do
  if command -v "${candidate}" >/dev/null 2>&1; then
    chrome_binary="$(command -v "${candidate}")"
    break
  fi
done
if [[ -z "${chrome_binary}" && -x "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" ]]; then
  chrome_binary="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
fi
if [[ -z "${chrome_binary}" ]]; then
  echo "Web smoke test could not find Chrome or Chromium." >&2
  exit 1
fi

temporary_directory="$(mktemp -d)"
server_pid=""
cleanup() {
  if [[ -n "${server_pid}" ]]; then
    kill "${server_pid}" >/dev/null 2>&1 || true
    wait "${server_pid}" >/dev/null 2>&1 || true
  fi
  rm -rf "${temporary_directory}"
}
trap cleanup EXIT

smoke_port="${WEB_SMOKE_PORT:-8769}"
smoke_url="http://127.0.0.1:${smoke_port}/"
python3 -m http.server "${smoke_port}" \
  --bind 127.0.0.1 \
  --directory "${site_directory}" \
  >"${temporary_directory}/server.log" 2>&1 &
server_pid="$!"

server_ready=false
for _ in {1..50}; do
  if curl --silent --fail "${smoke_url}" >/dev/null; then
    server_ready=true
    break
  fi
  sleep 0.1
done
if [[ "${server_ready}" != true ]]; then
  echo "Web smoke test server did not start." >&2
  cat "${temporary_directory}/server.log" >&2
  exit 1
fi

chrome_arguments=(
  --headless=new
  --no-sandbox
  --disable-dev-shm-usage
  --enable-unsafe-swiftshader
  --use-angle=swiftshader
  --user-data-dir="${temporary_directory}/chrome-profile"
  --virtual-time-budget=20000
  --timeout=30000
  --dump-dom
  "${smoke_url}"
)

smoke_timeout_seconds="${WEB_SMOKE_TIMEOUT_SECONDS:-45}"
if [[ ! "${smoke_timeout_seconds}" =~ ^[1-9][0-9]*$ ]]; then
  echo "WEB_SMOKE_TIMEOUT_SECONDS must be a positive integer." >&2
  exit 1
fi

chrome_status=0
if python3 - "${chrome_binary}" \
    "${temporary_directory}/dom.html" \
    "${temporary_directory}/chrome.log" \
    "${smoke_timeout_seconds}" \
    "${chrome_arguments[@]}" <<'PYTHON'
import os
import signal
import subprocess
import sys

chrome_binary = sys.argv[1]
dom_path = sys.argv[2]
log_path = sys.argv[3]
timeout_seconds = int(sys.argv[4])
command = [chrome_binary, *sys.argv[5:]]


def signal_process_group(process: subprocess.Popen[bytes], signal_number: int) -> None:
    try:
        os.killpg(process.pid, signal_number)
    except ProcessLookupError:
        pass


with open(dom_path, "wb") as dom_output, open(log_path, "wb") as log_output:
    process = subprocess.Popen(
        command,
        stdout=dom_output,
        stderr=log_output,
        start_new_session=True,
    )
    try:
        return_code = process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        signal_process_group(process, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            signal_process_group(process, signal.SIGKILL)
            process.wait()
        sys.exit(124)

    # Chrome can leave helper processes alive after the browser process exits.
    # They belong to the isolated session created above and are safe to stop.
    signal_process_group(process, signal.SIGTERM)
    if return_code < 0:
        sys.exit(min(255, 128 - return_code))
    sys.exit(return_code)
PYTHON
then
  chrome_status=0
else
  chrome_status=$?
fi

if [[ "${chrome_status}" -eq 124 ]]; then
  echo "Web smoke test timed out after ${smoke_timeout_seconds} seconds while waiting for Chrome." >&2
  tail -n 200 "${temporary_directory}/chrome.log" >&2 || true
  exit 1
fi
if [[ "${chrome_status}" -ne 0 ]]; then
  echo "Chrome exited with status ${chrome_status} during the Web smoke test." >&2
  tail -n 200 "${temporary_directory}/chrome.log" >&2 || true
  exit 1
fi

if ! grep -q '>Autosave ready</div>' "${temporary_directory}/dom.html"; then
  echo "Web game did not reach the clean-storage autosave-ready state." >&2
  cat "${temporary_directory}/chrome.log" >&2
  exit 1
fi

if grep -q '>Autosave starting</div>' "${temporary_directory}/dom.html"; then
  echo "Web game remained in its initial autosave state." >&2
  exit 1
fi

echo "Web clean-storage startup smoke test passed."
