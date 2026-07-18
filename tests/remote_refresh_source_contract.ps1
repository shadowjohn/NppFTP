$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\Windows\FTPWindow.cpp') -Raw
$refreshStart = $source.IndexOf('int FTPWindow::OnDirectoryRefresh(')
$refreshEnd = $source.IndexOf('int FTPWindow::OnError(', $refreshStart)
if ($refreshStart -lt 0 -or $refreshEnd -lt 0) {
	throw 'OnDirectoryRefresh source boundaries not found'
}

$refresh = $source.Substring($refreshStart, $refreshEnd - $refreshStart)
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
$hierarchyEnd = $source.IndexOf('FTPFile* files = (FTPFile*)queueData', $hierarchyStart)
$hierarchy = $source.Substring($hierarchyStart, $hierarchyEnd - $hierarchyStart)
if ($hierarchy -notmatch 'if\s*\(pendingNavigation\)\s*\{[\s\S]*OnDirectoryRefresh\(parent, curFTPDir->files, curFTPDir->count, NULL\)') {
	throw 'Stale hierarchy results can still mutate the cache model'
}

Write-Output 'remote_refresh_source_contract_exit=0'
