#!/usr/bin/env python3
import os
import re
import sys
from tempfile import TemporaryFile
from zipfile import ZipFile

import requests

RE_PREFIX = re.escape("https://www.argyllcms.com/Argyll_V")
RE_LATEST = re.compile(RE_PREFIX + r"\S+_src\.zip", re.IGNORECASE)


def get_download_url(mode: str):
    if mode == "develop":
        return "https://www.argyllcms.com/Argyll_dev_src.zip"
    if mode.startswith("V"):
        return "https://www.argyllcms.com/Argyll_%s_src.zip" % mode
    response = requests.get("https://www.argyllcms.com/downloadsrc.html")
    if not response.ok:
        return
    match = RE_LATEST.findall(response.text)
    if not match:
        return
    return match[0]


def main(blob_url: str):
    print("Downloading")
    response = requests.get(blob_url)
    if not response.ok:
        return
    print("Downloaded", response.url)
    print("Extracting")
    cache = TemporaryFile()
    cache.write(response.content)
    bundle = ZipFile(cache)
    common_path = os.path.commonpath(bundle.namelist())
    print("Write files")
    for path in set(map(os.path.dirname, bundle.namelist())):
        target_path = os.path.relpath(path, common_path)
        os.makedirs(target_path, exist_ok=True)
    for filename in bundle.namelist():
        target_path = os.path.relpath(filename, common_path)
        executable = filename.endswith(".sh") or filename.endswith(".ksh")
        with open(target_path, "wb") as fp:
            content = bundle.read(filename)
            fp.write(content)
            executable = executable or content.startswith(b"#!")
        if executable:
            os.chmod(target_path, 0o755)
    bundle.close()
    cache.close()
    print("Extracted")


if __name__ == '__main__':
    main(get_download_url(sys.argv[1]))
