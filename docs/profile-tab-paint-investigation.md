# Profile settings blank page investigation (2026-11-12)

## Report
Entering Hostname and pressing Tab can blank the page; switching tabs restores it.

## Candidate cause and change
`ProfilesDialog::OnInitDialog` creates the five page dialogs with `m_hwnd` as their parent, just like `IDC_TAB_PROFILEPAGE`. Their rectangles intentionally overlap. The tab resources had no `WS_CLIPSIBLINGS`, so invalidating the tab (e.g. keyboard focus/UI-state repaint) can paint into the page area. Tab switching re-shows/repaints the page, consistent with the reported recovery.

Both the full profiles dialog and single-profile dialog now give the tab `WS_CLIPSIBLINGS`. This changes painting only, not focus order, profile persistence, password handling or networking. No forced repaint timer/workaround was added.

## Verification
- `tests/profile_tab_paint_contract.ps1`: failed against the original resource and passed after the change; covers both dialog variants.
- Release x64 plugin rebuilt successfully using existing local dependencies.
- Existing seven PowerShell source-contract tests passed (including the new one).
- All nine existing Release test executables passed, including nine localhost-only connection-failure scenarios. No external server was contacted.
- Runtime/visual reproduction remains pending. Source contract is not proof of the visible symptom being resolved. The user's active Notepad++ session and profile data were not touched, and the candidate DLL has NOT been installed.

## Pending visual acceptance (isolated portable Notepad++, empty test profile)
1. Compare original and candidate DLL with the same Notepad++ version/theme/DPI.
2. New profile: type a dummy Hostname, Tab and Shift+Tab through every field; no page blanking.
3. Repeat in single-profile edit dialog, mouse focus, switch all five tabs, cover/uncover window.
4. Repeat light/dark themes and normal/scaled display if available.
5. Confirm Hostname and dummy settings survive close/reopen. Never use real credentials or connect during this test.

Candidate: `_build/Release/NppFTP.dll`. No commit or push performed.


## Visual attempt after user released desktop
Isolated portable copies were prepared in `_build/visual-profile-test/original` and `candidate` using only the installed Notepad++ executable and default language/style files, `doLocalConf.xml`, and the respective plugin DLL. No user connection config was copied. Original DLL SHA256: `80DEA22043C2B4867E933DF4B88011B94AFE356735985D6A3A707908BDC17CB8`; candidate: `3A7E240D4DD6F7539441E6DB951BADAFCB9988E8E3732D039EEAED57FAC77461`.

The original portable instance was launched through Explorer and its executable path verified. Computer Use repeatedly rejected binding its window with an application-owner mismatch, including after resetting its session (the reported expected/current application IDs were identical). No Host/Tab test could be executed. The original portable empty window remains open for manual testing. The installed application's DLL and real FTP configuration remain untouched. Visual acceptance is still pending, not passed.

## Candidate 1 rejected / candidate 2 (2026-11-12)
The user reproduced the blanking with candidate 1. Process/module inspection confirmed PID 26044 loaded the candidate DLL from the isolated candidate directory (SHA256 3A7E240D4DD6F7539441E6DB951BADAFCB9988E8E3732D039EEAED57FAC77461). Therefore this was not a wrong-DLL test. Candidate 1 is NOT a verified fix.

Candidate 2 adds explicit tab Z order (`SetWindowPos(hTab, HWND_BOTTOM, ..., SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)`) after creation of the sibling page dialogs. Sibling clipping only protects windows above the painting tab; the old layout code preserved Z order and never enforced this relationship. This remains a hypothesis pending actual Host/Tab verification, not a confirmed root cause.

Candidate 2 path: `_build/visual-profile-test/candidate2/notepad++.exe`. Plugin SHA256: 0283399F61E88F3C018E44DCE05B49CD4602AA3E009060E5F43C4B69F4EDEA87. Build passed with existing UTCP C4819 code-page warnings and existing empty-switch C4065 warning. The new Z-order source contract failed first and passed after the patch. No real settings were copied and no formal installed DLL changed.

## User acceptance: candidate 2 passed
The user explicitly reported acceptance passed for candidate 2 after retesting Hostname -> Tab, and supplied a screenshot of the single-profile "Profile settings" Connection tab. The screenshot's available image description/OCR shows the Hostname, connection type, Port, Username, Password, Timeout and initial-directory controls present, instead of the blank page. This is USER-PERFORMED acceptance of the reported reproduction, not an agent-driven CUA test.

At follow-up inspection no Notepad++ process was running, so the loaded DLL could not be independently rechecked at that moment. The candidate 2 artifact hash remains recorded above. Shift+Tab, all tab pages, the full profiles-list dialog, dark theme and alternate DPI are not yet independently visually verified; do not infer those coverage items from this acceptance.

Status: original Hostname -> Tab issue accepted fixed by user for candidate 2. Formal DLL installation, commit and push have not been performed. The source fix consists of sibling clipping in both dialog templates plus explicitly placing the tab behind the sibling content pages; no per-key forced-refresh workaround was required.

## Release preparation
The user authorized README updates, git push and release publication. Version v0.30.22-3wa.4 retains the accepted candidate 2 source change, with version metadata updated. Isolated test configurations remain excluded; the installed plugin is not replaced.
