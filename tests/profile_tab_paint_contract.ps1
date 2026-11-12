$ErrorActionPreference = 'Stop'
# The tab and its page dialogs overlap as siblings. A tab repaint must not
# paint over higher-Z-order page windows (including on keyboard UI-state changes).
$root = Split-Path -Parent $PSScriptRoot
$rc = Get-Content -LiteralPath (Join-Path $root 'src/Windows/NppFTP.rc') -Raw
foreach ($id in @('IDD_DIALOG_PROFILES', 'IDD_DIALOG_PROFILES_SINGLE')) {
    $block = [regex]::Match($rc, '(?ms)^' + $id + ' DIALOGEX.*?^END').Value
    if (!$block) { throw "Missing dialog resource: $id" }
    $tab = [regex]::Match($block, '(?m)^CONTROL\s+"",\s*IDC_TAB_PROFILEPAGE,\s*WC_TABCONTROL,\s*(?<style>[^,]+),').Groups['style'].Value
    if ($tab -notmatch '\bWS_CLIPSIBLINGS\b') {
        throw "${id}: overlapping sibling tab must clip page windows when repainting"
    }
}
Write-Output 'profile_tab_paint_contract: both profile dialogs clip overlapping sibling pages'

$source = Get-Content -LiteralPath (Join-Path $root 'src/Windows/ProfilesDialog.cpp') -Raw
if ($source -notmatch 'SetWindowPos\(hTab, HWND_BOTTOM, 0, 0, 0, 0,\s*SWP_NOMOVE\s*\|\s*SWP_NOSIZE\s*\|\s*SWP_NOACTIVATE\)') {
    throw 'The tab background must be behind its sibling page dialogs; clipping alone cannot protect lower siblings'
}
Write-Output 'profile_tab_paint_contract: tab is below sibling pages without moving focus'
