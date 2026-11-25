#!/usr/bin/env python3
"""Static contract checks for bounded HTTPS download progress reporting."""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
header = (root / "include/ps3scale/online.h").read_text(encoding="utf-8")
source = (root / "source/ps3/online_catalog.c").read_text(encoding="utf-8")

assert "ps3scale_online_download_limited_progress" in header
assert "ps3scale_online_download_progress" in header
assert "volatile size_t *downloaded_bytes" in header
assert "volatile size_t *total_bytes" in header
assert "if (downloaded_bytes || total_bytes)" in source
assert "CURLOPT_NOPROGRESS, 0L" in source
assert "CURLOPT_PROGRESSFUNCTION, report_download_progress" in source
assert "CURLOPT_PROGRESSDATA, &download" in source
assert "context->bytes_written += bytes;" in source
assert "*context->downloaded_bytes = context->bytes_written;" in source
assert "*context->total_bytes =" in source
assert "download_total > 0.0" in source
assert "download_total < 2147483647.0" in source
assert "ps3scale_online_download_limited_progress(" in source
main_source = (root / "source/ps3/main.c").read_text(encoding="utf-8")
assert "ps3scale_online_download_limited_progress(" in main_source
assert "&g_community_downloaded, &g_community_download_total" in main_source
assert "&g_ps2_config_downloaded, &g_ps2_config_download_total" in main_source
assert "return ps3scale_online_download_progress(url, destination, NULL, NULL);" in source
assert '#include <fcntl.h>' in source
assert 'open(temporary_path, O_WRONLY | O_CREAT | O_EXCL, 0600)' in source
assert 'temporary_created = 1;' in source
assert 'if (result != 0 && temporary_created)' in source
assert 'unlink(temporary_path);\\n    file = fopen' not in source
print("Online download progress contract: PASS")
