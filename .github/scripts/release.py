#!/usr/bin/env python3
import os
import re

import requests

RE_PREFIX = re.escape("https://www.argyllcms.com/Argyll_V")
RE_BLOB = re.compile(RE_PREFIX + r"\S+\.(?:tgz|zip)", re.IGNORECASE)


def download_links(platform: str):
    response = requests.get("https://www.argyllcms.com/download%s.html" % platform)
    if not response.ok:
        return
    match = RE_BLOB.findall(response.text)
    if not match:
        return
    return match


def main():
    links = []
    links += download_links("win")
    links += download_links("mac")
    links += download_links("linux")
    for link in links:
        print("Download", link)
        response = requests.get(link)
        if not response.ok:
            continue
        with open(os.path.basename(link), "wb") as fp:
            fp.write(response.content)


if __name__ == '__main__':
    main()
