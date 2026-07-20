#!/usr/bin/env python3

import hashlib
import hmac
import sys
import urllib.error
import urllib.request

from build_3rdparty import DEPENDENT_LIBS

CHUNK_SIZE = 1024 * 1024
TIMEOUT_SECONDS = 60


def verify_source(name, config):
    digest = hashlib.sha256()
    print('Verifying %s...' % name, end=' ', flush=True)
    try:
        with urllib.request.urlopen(config['url'], timeout=TIMEOUT_SECONDS) as response:
            while chunk := response.read(CHUNK_SIZE):
                digest.update(chunk)
    except (OSError, urllib.error.URLError) as exc:
        print('failed')
        print('%s: %s' % (name, exc), file=sys.stderr)
        return False

    if not hmac.compare_digest(digest.hexdigest(), config['sha256']):
        print('failed')
        print('%s: SHA-256 mismatch' % name, file=sys.stderr)
        return False

    print('checksum OK')
    return True


def main():
    success = True
    for name in sorted(DEPENDENT_LIBS, key=lambda item: DEPENDENT_LIBS[item]['order']):
        success = verify_source(name, DEPENDENT_LIBS[name]) and success
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
