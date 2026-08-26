# TODO

## Done

- [x] Record Codex Security scan summary in `history.md`.
- [x] Confirm current third-party downloads already have URL + SHA256 in `build_3rdparty.py`.

## 1. Third-party source ledger

- [x] Add `third_party_sources.md` with current source URL, SHA256, upstream project, license, and fallback notes for:
  - OpenSSL 4.0.1
  - zlib 1.3.2
  - libssh 0.12.0
  - bundled TinyXML 2.6.2-derived copy
  - bundled UTCP code
- [x] Verify each upstream URL and record the date checked.
- [x] Do not vendor tarballs into git yet.
- [x] Mirror tarballs only if builds must work offline or upstream archives become unstable.

## 2. Baseline build

- [x] Add `build.bat` and `build_scripts.ps1` to check the local build environment and run the selected baseline path.
- [x] Get one local build path working first: MinGW-w64 or Visual Studio, not both.
- [x] Record exact compiler/tool versions in `history.md`.
- [x] Keep dependency downloads hash-checked before any compile.

## 3. Security fixes, risk order

- [x] Fix FTPS hostname verification in `src/FTPClientWrapperSSL.cpp`.
- [x] Scope accepted FTPS certificate exceptions by host/profile, not global DER only.
- [x] Fix cache path traversal: reject `..`, canonicalize final path, enforce cache-root containment.
- [x] Fix FTP PASV parsing overflow: parse octets as bounded integers before formatting.
- [x] Fix FTP PASV endpoint policy: default data connection host to control peer.
- [x] Fix `CUT_StrMethods::RemoveCRLF` unsigned underflow.
- [x] Fix SFTP directory listing path composition overflow.
- [x] Replace default profile password storage with Windows DPAPI or equivalent.
- [x] Pin GitHub Actions by commit SHA and set explicit workflow permissions.
- [x] Add caps for FTP multiline responses and directory listings.
- [x] Fix review follow-ups: isolate release token, make FTP response cap fail closed, use SFTP path helper in production, avoid DPAPI fallback and secret test logs.

## 4. PSPad-style remote browser

- [x] Keep the old tree code until the new current-directory view is usable.
- [x] Add a current path display.
- [x] Add a quick search box for filtering the current directory list.
- [x] Add a change-directory combo box with recent paths and manual entry.
- [x] Show the current directory as a flat list: `..`, folders, files.
- [x] Wire double-click folder navigation and file open/download.
- [x] Persist only a small recent-directory list per profile.
- [x] Show prefix-matching recent directories while typing in Change dir.

## 4.1 PSPad-style remote browser polish

- [x] Fix remote browser panel resize/layout after dock shrink, dock expand, and splitter movement.
- [x] Add folder/file icons to the flat list.
- [x] Change list columns to PSPad order: `Name`, `Size`, `Modified`, `Type`, `Permissions`.
- [x] Enable header drag/drop so column order can be changed by the user.
- [x] Populate size, modified date, type, and permissions from `FileObject` metadata.
- [x] Add click feedback for directory/file activation with a temporary busy cursor.
- [x] Make Enter activate the focused flat-list directory or file exactly like double-click.
- [x] Make `Change dir` Enter navigate known directories and open known files.
- [x] Make unknown `Change dir` paths no-op without visible error.
- [x] Align FTP/current path/search/change-dir/list spacing with the PSPad reference screenshots.
- [x] Keep labels ASCII in source; track localized labels only as a separate resource/i18n task.
- [ ] Run manual Notepad++ QA for resize, icons, metadata columns, header drag, double-click/Enter activation, typed dir/file, and unknown path.

## 4.2 大型遠端目錄效能

目標：進入 `/tmp` 之類含有大量檔案的目錄時，Remote Browser 仍可立刻操作；不因建立清單、排序或更新舊 tree 讓 Notepad++ 視窗長時間無回應。

- [ ] 量測並記錄「收到 remote listing」、「建立 FileObject / 排序」、「灌入 flat list」、「舊 tree 更新」各階段耗時與檔案數，至少用 1k / 10k / 50k 筆 fixture 或可重播 listing 做 regression 基準。
- [ ] 將 flat remote list 改為 Windows virtual list (`LVS_OWNERDATA`)：保留排序後的 `FileObject` index，僅在可見列需要時提供名稱、圖示、Size、Modified、Type、Permissions，不再對每個遠端項目逐一 `ListView_InsertItem`。
- [ ] 對超過門檻的目錄採 display window：先顯示第一段項目與總筆數，再以 `Load more` 逐段擴充；Quick search、排序、focus restore 必須針對完整 model 運作，且清楚顯示目前已呈現範圍。
- [ ] 在 flat browser 模式避免為大型目前目錄同步展開或逐列更新 legacy tree；tree fallback 仍可用，但不能是目前目錄清單可操作前的必要工作。
- [ ] 對 Quick search 加 debounce 與可取消的過濾工作，避免每個 keypress 都掃描數萬筆並卡住 UI；結果更新後保留有效的選取與鍵盤 focus。
- [ ] focused tests：大量排序、display window 邊界、搜尋／排序組合、pending focus、`..` 固定首列、folder-first、metadata lazy formatting，以及舊 listing 晚到時不覆蓋目前目錄。
- [ ] Notepad++ 實機 QA：FTP／FTPS／SFTP 進入大量 `/tmp` 類目錄，確認可立即捲動、搜尋、Change dir、右鍵、切換其他目錄與中斷連線；記錄 1k / 10k / 50k 的可操作時間。
- [ ] 第二階段再評估 streaming directory listing：只有在 virtual list + display window 仍不足時，才讓 protocol worker 分批送 UI；FTP LIST / MLSD、FTPS、SFTP 都必須維持完整取消、上限與 ownership 規則，不先承諾伺服器端分頁。

## 5. Flat remote file operations

- [x] Add flat-list context menus, keyboard F2 rename, and current-directory blank-area commands.
- [x] Add multi-file picker and drag/drop target routing with session-only overwrite confirmation.
- [x] Add native checkbox-based CHMOD dialog with synchronized octal mode.
- [x] Add an owned recursive-upload planner with bounded paths and no reparse-point traversal.
- [x] Add remote scan and parent-first mkdir/upload queue operations with disconnect-safe batch ownership.
- [x] Add recursive local-directory upload, safe remote-directory merge, and per-file queue progress.
- [x] Show actionable single-operation mutation failures.
- [x] Show one failure summary for recursive uploads.
- [x] Route F2 rename through Notepad++ modeless-dialog key handling.
- [x] Preserve flat-list focus after a successful mutation refresh: focus the renamed item after rename, the same item after CHMOD, and the new file after creation; select it and scroll it into view.
- [ ] Run manual Notepad++ QA for flat-list menus, F2, picker/drop targets, Skip, Cancel, and session overwrite-all reset.
- [ ] Run manual SFTP/FTP QA for permission-denied, missing-path, and generic operation failure messages.
- [ ] Run real-server recursive-upload QA for FTP/SFTP target routing, new/existing directory merge, symlinks, nested collisions, every progress row, and one batch summary.

## 5.1 並行上傳排程

- [x] 新增全域 `1..8` 上傳 worker 設定，並以 normal priority 將手動與遞迴上傳交給並行 scheduler。
- [x] 將遠端 cache 檔案儲存設為 urgent；同檔仍在等待時直接提升，已在 active 時只保留一筆後續上傳。
- [ ] 使用 Notepad++ 搭配 FTP/FTPS/SFTP 實機驗證 limit `1`/`2`、手動／遞迴上傳、Abort／取消、儲存提升／active follow-up 與傳輸中斷線。

## 5.2 大量上傳佇列控制（下一刀）

目標：在下方 transfer list 的 upload 項目按右鍵，提供接近 FileZilla 的大量上傳控制；不影響下載、遠端瀏覽或檔案 mutation queue。

- [ ] 在 upload row 或 transfer list 空白處的右鍵選單加入一組全域上傳控制：正常運作時只顯示 `Pause all uploads`；已暫停時只顯示 `Resume uploads`。
- [ ] 第一版暫停採 scheduler dispatch gate：停止派送新的 waiting upload 給空閒 worker；已在傳輸中的檔案繼續完成，不對 FTP／FTPS／SFTP data stream 做不安全的強制 suspend。恢復後沿用原有 normal / urgent priority 與同檔 conflict 規則。
- [ ] 顯示暫停狀態並在 Output 記錄 pause / resume；waiting rows 保留在清單中，進度不會假裝仍在前進。暫停狀態只存在目前 session，disconnect / reconnect 後回到正常派送。
- [ ] 在 upload row 的右鍵選單加入 `Clear selected upload task`：waiting 項目立即取消並移除；active 項目走既有 abort / terminal accounting，最後顯示 canceled，不誤報為成功或失敗。
- [ ] 在 upload row 或 transfer list 空白處加入 `Clear all upload tasks`：取消所有 waiting upload，並對所有 active upload 發 abort；不得影響 download、directory preparation、remote browser 或其他序列 queue operation。
- [ ] 保留既有 recursive upload batch 的 selected / success / failed / canceled accounting 與單一 summary；clear selected / clear all、scheduler rejection、teardown 與延遲 End/Remove notification 都不能造成重複摘要、reference leak 或 UI deadlock。
- [ ] focused tests：暫停後 active worker 可正常結束但不再派送 waiting upload；resume 後 urgent 仍先於 normal；清除 waiting / active 單筆與 clear all 都恰好一次 terminal accounting；paused 狀態下 disconnect / teardown 安全收斂。
- [ ] Notepad++ 實機 QA：limit `1` / `2`、normal + urgent cache save、手動與遞迴上傳、waiting / active selected clear、clear all、pause / resume、FTP／FTPS／SFTP 及傳輸中 disconnect；確認選單文字依狀態互斥且不影響下載。

## 6. 多國語系

- [ ] 後續另行加入完整 UI 語系選擇；預設語系規劃為正體中文。

## 7. User-directed remote download

- [x] Route toolbar and flat-list Download commands through a Save As/folder-picker workflow; keep Edit cache-only.
- [x] Download a selected remote directory recursively into a same-named local root.
- [x] Guard local output paths, skip remote symlinks, and resolve file collisions per batch.
- [x] Show per-file queue progress and one directory-download failure summary.
- [x] Add focused plan tests.
- [x] Manual QA: recursively download a remote directory to a selected local parent in Notepad++.
- [ ] Run remaining manual QA: file Save As cancel/success/no-open; overwrite/skip/all/cancel; local file/directory conflicts; scan failure; FTP/FTPS/SFTP queue progress and one summary.

## Not now

- [ ] Do not rewrite the whole FTP window before the flat browser works.
- [ ] Do not add new UI libraries.
- [ ] Do not self-host third-party source archives unless reproducible/offline release builds need it.
