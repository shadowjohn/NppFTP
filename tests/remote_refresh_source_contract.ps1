$ErrorActionPreference = 'Stop'

function Get-BracedBlock([string]$Text, [string]$Marker, [int]$StartAt = 0) {
	$start = $Text.IndexOf($Marker, $StartAt)
	if ($start -lt 0) {
		throw "Source marker not found: $Marker"
	}

	$open = $Text.IndexOf('{', $start)
	if ($open -lt 0) {
		throw "Opening brace not found: $Marker"
	}

	$depth = 0
	for ($i = $open; $i -lt $Text.Length; $i++) {
		if ($Text[$i] -eq '{') {
			$depth++
		} elseif ($Text[$i] -eq '}') {
			$depth--
			if ($depth -eq 0) {
				return $Text.Substring($start, $i - $start + 1)
			}
		}
	}

	throw "Closing brace not found: $Marker"
}

$source = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\Windows\FTPWindow.cpp') -Raw
$refresh = Get-BracedBlock $source 'int FTPWindow::OnDirectoryRefresh('
$cacheOnly = $refresh.IndexOf('if (!isFinalTarget) {')
if ($cacheOnly -lt 0) {
	throw 'Cache-only hierarchy refresh can reach destructive child deletion'
}
$cacheReturn = $refresh.IndexOf('return 0;', $cacheOnly)
$staleReturn = $refresh.IndexOf('if (!updateVisibleUi)', $cacheReturn)
$destructiveDelete = $refresh.IndexOf('parent->RemoveAllChildren(false);')
$treeFill = $refresh.IndexOf('m_treeview.FillTreeDirectory(parent);')
$ensureVisible = $refresh.IndexOf('m_treeview.EnsureObjectVisible(parent);')

if ($cacheReturn -lt 0 -or $cacheReturn -gt $destructiveDelete) {
	throw 'Cache-only hierarchy refresh can reach destructive child deletion'
}
if ($refresh.Substring($cacheOnly, $cacheReturn - $cacheOnly) -match 'RemoveAllChildren|FillTreeDirectory') {
	throw 'Cache-only hierarchy refresh deletes selected tree descendants'
}
if ($staleReturn -lt 0 -or $staleReturn -gt $destructiveDelete) {
	throw 'Stale final refresh can mutate the retained view model'
}
if ($ensureVisible -lt 0 -or $ensureVisible -gt $treeFill) {
	throw 'Eligible final refresh cannot materialize its hierarchy target'
}

$hierarchyStart = $source.IndexOf('std::vector<FTPDir*> parentDirObjs')
$hierarchy = Get-BracedBlock $source 'if (pendingNavigation)' $hierarchyStart
if ($hierarchy -notmatch 'OnDirectoryRefresh\(parent, curFTPDir->files, curFTPDir->count, NULL\)') {
	throw 'Stale hierarchy results can still mutate the cache model'
}

$synthetic = @'
if (pendingNavigation) {
	int untouched = 0;
}
OnDirectoryRefresh(parent, curFTPDir->files, curFTPDir->count, NULL);
'@
$syntheticPending = Get-BracedBlock $synthetic 'if (pendingNavigation)'
if ($syntheticPending -match 'OnDirectoryRefresh') {
	throw 'Balanced block extraction accepted a call outside pendingNavigation'
}

$sslSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\FTPClientWrapperSSL.cpp') -Raw
$getDir = Get-BracedBlock $sslSource 'int FTPClientWrapperSSL::GetDir('
$outputInit = $getDir.IndexOf('*files = NULL;')
$listRequest = $getDir.IndexOf('m_client.GetDirInfo')
if ($outputInit -lt 0 -or $outputInit -gt $listRequest) {
	throw 'FTP GetDir does not initialize its output pointer before listing'
}
$emptyResult = Get-BracedBlock $getDir 'if (!vfiles.size())'
if ($emptyResult -notmatch 'return\s+OnReturn\(0\);') {
	throw 'FTP GetDir does not report a successful empty directory'
}

$queueSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\QueueOperation.cpp') -Raw
$perform = Get-BracedBlock $queueSource 'int QueueGetDir::Perform()'
if ([regex]::Matches($perform, 'FTPFile\s*\*\s*files\s*=\s*NULL\s*;').Count -ne 2) {
	throw 'QueueGetDir does not initialize both wrapper output pointers'
}

$fillRemoteList = Get-BracedBlock $source 'int FTPWindow::FillRemoteList()'
if ($fillRemoteList -notmatch 'if\s*\(parent\s*&&\s*!m_remoteCurrentDir->isRoot\(\)\)') {
	throw 'Remote list still inserts the parent item at root'
}
$backspace = Get-BracedBlock $source 'if (key->wVKey == VK_BACK)'
if ($backspace -notmatch '!m_remoteCurrentDir->isRoot\(\)') {
	throw 'Remote Backspace can still navigate from root to its self-parent'
}

$treeHeader = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\Windows\Treeview.h') -Raw
if ($treeHeader -match 'ExpandDirectory\([^;]*selectTreeItem') {
	throw 'Treeview::ExpandDirectory still exposes the dead selectTreeItem parameter'
}
$treeSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\Windows\Treeview.cpp') -Raw
$expandDirectory = Get-BracedBlock $treeSource 'int Treeview::ExpandDirectory('
if ($expandDirectory -match 'selectTreeItem') {
	throw 'Treeview::ExpandDirectory still contains the dead selection branch'
}
if ($source -match 'ExpandDirectory\(parent, NULL, updateVisibleUi\)') {
	throw 'FTPWindow still calls the removed three-argument ExpandDirectory interface'
}

Write-Output 'remote_refresh_source_contract_exit=0'
