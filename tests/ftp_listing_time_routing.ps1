$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath 'src/FTPClientWrapperSSL.cpp' -Raw

if ($source -notmatch '#include\s+"ftp_listing_time_utils\.h"') {
    throw 'FTP listing time helper must be included by the FTP/FTPS wrapper.'
}

if ($source -notmatch 'ftp_listing_local_system_time_to_filetime\(st,\s*&ft\)') {
    throw 'FTP/FTPS listing timestamps must normalize local wall-clock time before storage.'
}

Write-Output 'ftp_listing_time_routing_exit=0'
