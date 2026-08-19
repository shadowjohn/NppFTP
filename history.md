# NppFTP 接續紀錄

## 2026-08-18 Final concurrent upload integration fixes

- `FTPSession::Clear()` 不再重複使用同一個 single-use `QueueDisconnect`。transfer wrapper 與 main wrapper 各自使用獨立 operation，兩者都會實際呼叫 `Disconnect()`；只由 main wrapper 的 operation 保留既有一次 UI disconnect 通知，避免 SFTP main session cleanup 被第二次 `BeginPerform()` 拒絕。
- `QueueRemoteUploadPrepare` 在每個 directory 開始前，以及 `Connect`、`MkDir`、`GetDir` 返回後檢查 operation 自己的 persistent canceled state。第一個 `MkDir` 期間 Abort／teardown 即使被 wrapper `OnReturn` 清掉 abort flag，也不會繼續建立後續目錄、派送 selected files 或重複 terminal accounting。
- `FTPWindow::OnDisconnect()` 會把 `m_activeTransferCount` 與 `m_busy` 歸零後再更新 toolbar；既有 End handler 只在 count 大於零時遞減，因此 teardown suppress 的 End 不會讓 reconnect 維持假 busy，延遲 End 也不會把 count 減成負數。
- TDD red 證據：routing contract 先失敗於 `Clear must use a dedicated disconnect operation for the transfer wrapper.`；雙目錄 queue lifecycle 先失敗於 `prepare Abort prevents later MkDir calls`；暫時移除 UI reset 的 mutation 會失敗於 `Disconnect must reconcile active transfer count and busy state before refreshing the toolbar.`。
- focused Release checks 通過：`ftp_settings_roundtrip_exit=0`、`concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`certificate_store_lock_exit=0`、`ftp_session_upload_routing_exit=0`；`build.bat -Arch x64 -Config Release` 成功。DLL SHA256 `29D675A3A733A3DC456A5BB6F029588EFF3682298773D5A1E4D37D6A7049203D`，ZIP SHA256 `325AD589721E898581EC37FB8DF953EC53919C8A0C853A80CD15503E143C5ACD`。Task 5 cached-save urgency 未修改；live Notepad++ + FTP/FTPS/SFTP disconnect／Abort／reconnect QA 仍是外部邊界。

## 2026-08-18 Task 5 review fix round 2

- `TestActiveSaveUrgentFollowUp` 移除固定 `PumpFor(150)`。active same-file upload 與 urgent follow-up 入列後，測試再 enqueue 一筆 non-conflicting `active-probe.txt`，等待第二個 worker 真正進入 probe `SendFile` barrier，才檢查 scheduler 已掃描 lanes、跳過 conflicting urgent 並選中 probe。
- probe active 時明確驗證 queue size 為 3、active worker 為 2、same-file 傳輸仍只有原始 1 次、follow-up 尚未執行且 Abort 為 0；釋放 probe 後再等待 state 收斂為 queue size 2／active 1，證明只剩原 active 與一筆 urgent follow-up。釋放原 active 後，同檔總傳輸仍恰為 2 次。
- deterministic mutation check 暫時略過 production `FindWorkerConflictLocked` 後，probe 已確實開始，測試穩定失敗於 `scheduler keeps conflicting urgent follow-up waiting while probe is active`；mutation 已還原，production source 無本輪 diff。
- focused Release checks 全部通過：`ftp_settings_roundtrip_exit=0`、`concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`certificate_store_lock_exit=0`、`ftp_session_upload_routing_exit=0`。
- `build.bat -Arch x64 -Config Release` 通過；`NppFTP.dll` 4,772,864 bytes，SHA256 `3C7657DD922FE8882511B271C7F2AF633C376D0D0D78F24507FE1B57A52DACE8`；ZIP 2,204,294 bytes，SHA256 `CB9B614B7E7F40A387985236E47D89EB29F74EF3C6292A244BDE5C5FAB70130E`。只保留既有 CMake policy 與 UTCP code-page warnings；未執行 live Notepad++ + FTP/FTPS/SFTP QA，也未修改 production、`OnSave` 或 `PluginInterface`。

## 2026-08-18 Task 5 review fix round 1

- 移除 `tests/concurrent_upload_scheduler.cpp` 自行重做 promotion／follow-up 的 `SavePriorityPolicyHarness`；該模型沒有呼叫 production `ConcurrentUploadScheduler::AddQueueOp()`，也無法覆蓋 `QueueUpload::Equals` duplicate matching 或 active worker 的 `ConflictsWith` 判定。
- 真實 lifecycle focused test 改以 controllable `FakeWrapper` barrier 呼叫 production scheduler。單 worker 案例先卡住 active normal upload，再把 normal tail 排在 manual same-file upload 前，確認 urgent save 只提升原項目、重複 save 不增加 queue，active 不被 Abort，釋放後 save 仍先於 tail 且只傳一次。
- 雙 worker 案例先卡住 active same-file normal upload，再 enqueue 兩次 urgent save；空閒 worker 仍因 same-file conflict 不得同時傳輸，queue 只保留一筆 follow-up。釋放 active 後同檔總傳輸次數恰為兩次，且 scheduler 從未呼叫 Abort。
- mutation checks 證明測試直接約束 production：停用 `PromoteWaiting` 時失敗於 `promoted save runs before earlier normal tail`；略過 `FindWorkerConflictLocked` 時失敗於 `active upload retains exactly one waiting follow-up`；略過 urgent duplicate merge 時失敗於 `waiting promotion keeps one active and two waiting uploads`。三個 mutation 均已還原，`src/ConcurrentUploadScheduler.cpp` 無最終 diff。
- `todo.md` 的 Task 5 與完整多國語系待辦已改為正體中文；README 明列 limit `1`／`2`、waiting promotion、active follow-up、Abort/cancel 與 active-transfer disconnect 都尚未完成 live Notepad++ + FTP/FTPS/SFTP QA。
- focused Release checks 全部通過：`ftp_settings_roundtrip_exit=0`、`concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`certificate_store_lock_exit=0`、`ftp_session_upload_routing_exit=0`。
- `build.bat -Arch x64 -Config Release` 通過；`NppFTP.dll` 4,772,864 bytes，SHA256 `09EE52EA9DBE86070752C5B8A744E591CC27C078F7DA399139A391C4556BF91D`；ZIP 2,204,294 bytes，SHA256 `5626619F8E5A78DE1C142C1211A5A0AA304F98762DBD148AD6F347AF29E37C52`。只保留既有 CMake policy 與 UTCP code-page warnings；`OnSave`／`PluginInterface` 未修改，也未執行或宣稱 live protocol QA。

## 2026-08-18 Make cached remote-file saves urgent

- 只將 `FTPSession::UploadFileCache` 改為呼叫 `UploadFile(..., UploadPriorityUrgent)`；`NppFTP::OnSave`、`PluginInterface`、一般手動上傳與遞迴上傳維持既有流程，manual/recursive upload 仍是 normal priority，下載、瀏覽與 mutation 仍走序列 queue。
- 儲存遠端 cache 檔案不會中斷 active upload，urgent 項目會取得下一個空閒 worker 並排在等待中的 normal upload 前面。同檔 normal upload 尚在等待時會直接提升而不重複；同檔正在 active 時只保留一筆 urgent follow-up，後續再次儲存沿用該筆等待項目。
- TDD routing contract 先失敗於 `Automatic saves must use urgent upload priority.`；補上唯一 production 引數後，`pwsh -NoProfile -File .\tests\ftp_session_upload_routing.ps1` 輸出 `ftp_session_upload_routing_exit=0`。scheduler focused test 也覆蓋 waiting promotion、active follow-up 與重複 save 不新增第二筆 follow-up。
- focused Release checks 全部通過：`ftp_settings_roundtrip_exit=0`、`concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`certificate_store_lock_exit=0`、`ftp_session_upload_routing_exit=0`。
- `build.bat -Arch x64 -Config Release` 通過；`NppFTP.dll` 4,772,864 bytes，SHA256 `65107A71C530132CB9250101185FB281D6D79F80CE094639407A93B6FD0AF9FE`；ZIP 2,204,294 bytes，SHA256 `40D4B35D3413082662D9F136B54027BE0E71354804D79F68136B387E624BC080`。僅保留既有 CMake policy 與 UTCP code-page warnings。
- 未執行 live Notepad++ + FTP/FTPS/SFTP 手動 QA。仍須確認 Global settings 正體中文與重啟 persistence、limit `1`/`2` 的 save 插隊順序、manual/recursive upload、Abort 全部 active、取消單筆 waiting、waiting-save promotion、active-save follow-up，以及 active transfer 中乾淨 disconnect。

## 2026-08-18 Task 4 atomic completion handoff review fix

- 遞迴上傳最後一個 selected file 到達 terminal 時，不再由 worker thread 把 `QueueRemoteUploadComplete` 塞進另一條會等待 UI acknowledgement 的 queue；`RemoteUploadBatch` 改以 `AddRef + PostMessage` 非阻斷通知 FTPWindow，再由 UI thread 驗證 session generation 後建立原有 completion marker。
- `FTPSession::Clear()` 會先遞增 generation、關閉 transfer queue teardown gate，才等待 upload scheduler/worker 結束；舊 generation 的 completion payload 只做 `Release`，不顯示摘要。worker 全停後，Clear 會清掉尚未處理的 payload，避免 reference leak 或已清除 session 被延遲存取。
- `FTPQueue::AbortActive()` 用原子 execution state 只認領仍在 `Perform()` 的操作；已算出 success/server failure、正等待 End/Remove 的 late Abort 是 no-op，不會覆寫結果。serial prepare 也走相同 active-abort 路徑，Abort 後不會派送 selected files。
- scheduler rejection/merge/cleanup 與 queue teardown 仍經既有 terminal hook 完成 selected/success/failed/skipped/canceled accounting；正常 session 只發一個摘要，confirmed teardown 才 suppress 摘要。
- 新增真實 Win32 queue/message-ack lifecycle 測試，覆蓋正常與 late Abort、active upload Abort、prepare Abort、teardown generation drop、scheduler rejection、zero-selected exactly-once marker；不再只以 counter helper 代替行為驗證。
- focused Release checks 通過：`concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`ftp_session_upload_routing_exit=0`。
- `build.bat -Arch x64 -Config Release` 通過；`NppFTP.dll` 4,771,328 bytes，SHA256 `A4842E8D6B46FECB9CB288D8D59414E7491CD7ECBCF521FCAB7886F50E98DE5F`；ZIP 2,203,500 bytes，SHA256 `282F011E8F79401A18D8F4FE52A59ED6ECE281146FF6436A8DED65CADE973F20`。
- 尚未執行 Notepad++ 搭配真實 FTP/FTPS/SFTP server 的 disconnect-during-upload 手動 QA；build 只剩既有 UTCP code-page 與 legacy conversion warnings。

## 2026-08-17 Task 4 recursive upload terminal lifecycle review fix

- 遞迴上傳的 scheduler 拒絕、重複合併、等待取消、worker shutdown 與 active Abort，現在都會先經過共用 terminal hook；每個 selected file 恰好計入一次 success、failed 或 canceled，remaining counter 不會卡住。
- `QueueRemoteUploadPrepare` 發生 fatal `-1` 時不再派送檔案，會把尚未開始的 selected files 記為 canceled，最後只產生一次真實摘要；active Abort 與真正的 server failure 也分別記為 canceled / failed。
- `GetDirectoryItems()` 會依遠端路徑深度穩定排序，不再依賴掃描輸入順序，確保 parent directory 一定早於 child directory 準備。
- 已確認 session teardown 的特殊邊界：所有 batch terminal accounting 與 reference release 仍完整執行，但 suppress `QueueRemoteUploadComplete` UI marker，避免 UI thread 等 shutdown、worker 又等待 UI ack 的 deadlock，也避免摘要存取已清除的 session。
- 一般 scheduler rejection、手動等待取消、session 保持開啟時的 active Abort、prepare fatal failure，仍會送出恰好一次摘要；沒有加入 Task 5 urgent save、設定 UI 或多國語系改動。
- focused Release checks 通過：`remote_upload_plan_exit=0`、`concurrent_upload_scheduler_exit=0`、`ftp_session_upload_routing_exit=0`。`remote_upload_plan.cpp` 已移除 Release 下失效的 `assert`，並加入可直接重跑的 CMake target。
- `build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll`（4,770,816 bytes）與 `_build\NppFTP-0.30.22-win64.zip`（2,203,178 bytes）；ZIP SHA256：`436663F5A2A388833F356AD071F4F87D3D5DA49B5DFFBF24443872C5AA301471`。
- 尚未執行 Notepad++ 加真實 FTP/FTPS/SFTP server 的手動 QA；build 只剩既有 UTCP code-page 與 legacy conversion warnings。

## 2026-07-18 Flat remote list navigation, context targets, and sorting

- Flat list 的 Backspace 現在先對上層做 LIST，只有最新且成功的請求才切換目前目錄；root 是 no-op。失敗或 stale completion 保留目前路徑、清單、選取與 wait cursor。
- 檔案／空白列的建立資料夾或空白檔案維持在目前目錄；從資料夾列建立時改在該資料夾內。`..` 不提供 mutation menu。
- Name、Size、Modified、Type、Permissions 支援單一欄位 asc/desc 排序與原生箭頭；`..` 固定第 0 列、資料夾固定在檔案前，Size 依原始 64-bit 值比較。
- 本輪重新編譯並通過：`file_size_64_exit=0`、`remote_browser_utils_exit=0`、`remote_list_keyboard_exit=0`、`remote_list_sort_exit=0`、`utcp_transfer_buffer_exit=0`、`recent_dirs_exit=0`、`remote_refresh_source_contract_exit=0`。
- `build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll`（4,716,032 bytes）與 `_build\NppFTP-0.30.22-win64.zip`（2,159,225 bytes）；ZIP SHA256：`AA5A5B4991E54E6427D4DAF2DA545FB945CF21F426F4B86AB7E1355C1502AF74`。
- `copyNppFTPdllToRealENV.bat` 在非系統管理員環境因 `C:\Program Files` Access is denied 失敗，未覆蓋既有 plugin DLL；FTP/FTPS、SFTP 與 Notepad++ 實機 acceptance matrix 仍待執行。

## 2026-07-18 Add 64-bit file size support

- FTP / SFTP 檔案大小與傳輸進度現在會保留超過 2 GB 的 64-bit 值。
- Size 使用 B / KB / MB / GB / TB、兩位小數並維持靠右顯示。
- focused regressions：`file_size_64_exit=0`、`remote_browser_utils_exit=0`、`remote_list_keyboard_exit=0`、`utcp_transfer_buffer_exit=0`、`recent_dirs_exit=0`。`recent_dirs.exe` 以 VS x64 與 `/DWIN32_LEAN_AND_MEAN`、`/I .` 相容前置旗標重編後執行。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll`（4,708,352 bytes）與 `_build\NppFTP-0.30.22-win64.zip`（2,155,585 bytes）；ZIP SHA256：`668AEB810B1C6D20463BED47F14D5508C4FF05F9C5189E43415053DCCF37BA5D`。

## 2026-07-09 早上：初次理解專案

### 專案定位

- NppFTP 是 Notepad++ 的 Windows 外掛，提供 FTP、FTPS、FTPES、SFTP 連線、檔案瀏覽、下載、上傳、快取對映與佇列傳輸。
- 主要產物是 `NppFTP.dll`。程式是 Win32/C++，使用 Notepad++ plugin API、Win32 UI、TinyXML、UTCP、OpenSSL、zlib、libssh。
- `tinyxml/` 與 `UTCP/` 是 repo 內含原始碼；OpenSSL、zlib、libssh 由 `build_3rdparty.py` 下載並建置。

### 建置重點

- `CMakeLists.txt` 會先跑 `build_3rdparty.py` 建第三方相依，再把 `src/`、`src/Windows/`、`tinyxml/`、`UTCP/` 一起編成 module library。
- `Makefile.mingw` 支援 MinGW-w64，`BITS=32` 產 x86，`BITS=64` 產 x64。
- `BUILDING.md` 提到 Windows 可用 Visual Studio 2019+，Linux/Windows 可用 MinGW-w64；需要 Python 3、Perl、CMake。
- 目前 `build_3rdparty.py` 釘住：
  - OpenSSL 4.0.1
  - zlib 1.3.2
  - libssh 0.12.0
- GitHub Actions build matrix 包含 Windows 2022 的 `x64`、`ARM64`、`Win32`，以及 Ubuntu 24.04 cross-build。

### 主要入口

- `src/PluginInterface.cpp`
  - Notepad++ 外掛 callback 集中在這裡：`setInfo`、`getName`、`getFuncsArray`、`beNotified`、`messageProc`、`isUnicode`。
  - menu items 共 4 個：Show NppFTP Window、Focus NppFTP Window、About NppFTP、Upload Current File。
  - `NPPN_READY` 取得 Notepad++ plugin config dir 後呼叫 `nppFTP.Start(...)`。
  - `NPPN_SHUTDOWN` 呼叫 `nppFTP.Stop()`。
  - `NPPN_FILESAVED` 用 buffer id 取目前完整路徑，交給 `NppFTP::OnSave`。
  - `NPPN_BUFFERACTIVATED` 交給 `NppFTP::OnActivateLocalFile` 更新 UI 上傳狀態。

- `src/NppFTP.cpp`
  - `Start` 建立 `%Notepad++ plugin config%\NppFTP`，設定全域 `_ConfigPath` 與 `_HostsFile`。
  - 載入 `NppFTP.xml`、`Certificates.xml`，建立 `FTPWindow` 與 `FTPSession`。
  - `OnSave` 會從本機快取路徑解析 `username@hostname`，必要時自動連到符合的 profile，然後呼叫 `UploadFileCache`。
  - `SaveSettings` 會寫回 settings、profiles、certificates。

### Runtime 分層

- UI 層：`src/Windows/FTPWindow.*`
  - 管 dockable window、toolbar、treeview、queue window、output window、profile/settings dialogs、drag/drop。
  - 使用者點 toolbar/menu 後，多數操作交給 `FTPSession` 排入 queue。
  - queue 完成後，`FTPWindow::OnEvent` 依 operation type 更新 tree、打開下載檔、刷新遠端目錄或顯示錯誤。

- Session 層：`src/FTPSession.*`
  - 一個 session 有兩組 wrapper/queue：
    - `m_mainWrapper` + `m_mainQueue`：連線、目錄、rename、chmod、mkdir、delete 等操作。
    - `m_transferWrapper` + `m_transferQueue`：下載、上傳、copy 等傳輸。
  - `StartSession` 從 `FTPProfile::CreateWrapper` 建立 protocol wrapper，並設定 profile cache 的環境變數。
  - `Connect` 會 queue `QueueConnect`；如果 profile 有 NOOP interval，會建 timer queue。
  - `TerminateSession` 會確認未完成傳輸、刪 timer、清 queue、disconnect wrapper。

- Queue 層：`src/FTPQueue.*`、`src/QueueOperation.*`
  - `FTPQueue::Initialize` 會開 Win32 thread 跑 `QueueLoop`。
  - `QueueOperation::SendNotification` 透過 window message 通知 UI thread，跨 thread 時會等待 UI ack。
  - 同類且同目標的 operation 會用 `Equals` 去重。

- Protocol 層：`src/FTPClientWrapper*.cpp`
  - `FTPProfile::CreateWrapper` 依 security mode 建 wrapper：
    - `Mode_SFTP` -> `FTPClientWrapperSSH`
    - `Mode_FTP` / `Mode_FTPS` / `Mode_FTPES` -> `FTPClientWrapperSSL`
  - SFTP 走 libssh/libssh-sftp，known hosts 位置由 `_HostsFile` 控制。
  - FTP/FTPS/FTPES 走 UTCP/CUT_FTPClient，SSL certificate trust 存在 `Certificates.xml`。

- 設定與快取：`src/FTPSettings.*`、`src/FTPProfile.*`、`src/FTPCache.*`
  - Global cache 預設 `%CONFIGDIR%\Cache\%USERNAME%@%HOSTNAME%\%PORT%` 對應遠端 `/`。
  - Profile cache 會接到 global cache parent；查找是「profile map 由上到下先符合先用，最後才 global」。
  - 下載：遠端路徑 -> 本機 cache path；上傳：本機 cache path -> 遠端路徑。

### 改動時先看的檔案

- Notepad++ lifecycle/menu/notification：`src/PluginInterface.cpp`
- 外掛啟停、settings XML、存檔自動上傳：`src/NppFTP.cpp`
- 連線/session/queue 調度：`src/FTPSession.cpp`
- queue thread、notification ack：`src/FTPQueue.cpp`、`src/QueueOperation.cpp`
- protocol 實作：`src/FTPClientWrapperSSL.cpp`、`src/FTPClientWrapperSSH.cpp`
- profile/settings/cache：`src/FTPProfile.cpp`、`src/FTPSettings.cpp`、`src/FTPCache.cpp`
- UI 事件與 tree/queue/output：`src/Windows/FTPWindow.cpp`
- path/encoding helper：`src/PathUtils.cpp`、`src/StringUtils.cpp`

### 踩雷點

- 專案混用 `TCHAR`、UTF-8 XML、`CP_ACP` 轉換與手動配置字串；不要整檔重寫或任意改檔案編碼。
- 多數 path buffer 是 `MAX_PATH` 與 raw pointer，改 path/字串邏輯要小心長度、釋放責任與 NUL 結尾。
- queue notification 有跨 thread ack；不要在 UI thread 做會反等 worker 的長操作，容易卡住。
- `FTPCache::ClearCurrentCache` 會刪除目前映射到的 cache 目錄；改 cache map 前先確認不會誤刪使用者資料。
- `OnSave` 自動上傳依本機路徑中的 `username@hostname` 判斷 profile，若與目前連線不符會 terminate session。
- `TODO.txt` 既有提醒：timeout 可能造成 UI lock、printf 系列 buffer 不理想、TCHAR/CP_ACP 效率差、treeview drag、MSVC 下 ssh_userauth_none heap corruption。

### 本次狀態

- 只新增這份 `history.md`，沒有改程式碼。
- 尚未建置；本次是 repo 理解與接續紀錄。

## 2026-07-09 Codex Security 掃描紀錄

- 針對「是否有藏步」跑完 repository-wide standard security scan；未看到明確後門、隱性 C2 或刻意外連邏輯。
- 最終報告：`C:\Users\johnho\AppData\Local\Temp\codex-security-scans-gCSPkX\NppFTP\8b0e0e16846442ecc69978784eee5b9169e8726f_20260709T012657Z_qlmd309h\report.md`。
- 最終 10 筆 finding：high 1、medium 5、low 4。最高風險是 FTPS 憑證沒有做 hostname binding；其他重點是 SFTP/FTP parser memory safety、cache path traversal、PASV data endpoint 可被遠端指定、profile 密碼硬編碼 DES key、CI action mutable tag。
- 本次沒有 Windows C++ compiler，因此 memory corruption 類只做 static source trace，尚未動態 crash harness / exploit 驗證。
- `CAND-014` unqualified `UxTheme.dll` load 保留為 hardening note；沒有證明 Notepad++ 啟動路徑有可寫 DLL search-path 前置條件，所以不列正式 finding。
- 工作台 complete 時曾因掃描後出現的 untracked `snapshot/orin.png` 判定 worktree 變動而拒絕封存；已暫移到 scan dir 後完成封存，再原路復原該圖檔。

## 2026-07-09 接手 TODO

- 新增 `todo.md`，先做 third-party source ledger，再建立一條 baseline build，接著按風險修 security findings，最後做 PSPad-style remote browser。
- 第三方 tarball 先只記 URL/hash/source，不放進 git；只有離線或 reproducible release build 真的需要時才自養 mirror。

## 2026-07-09 第三方來源帳本

- 新增 `third_party_sources.md`：記錄 OpenSSL 4.0.1、zlib 1.3.2、libssh 0.12.0 的下載 URL/hash/license，也記錄 vendored TinyXML 與 UTCP 的 repo 子樹 digest。
- 2026-07-09 檢查下載來源：OpenSSL、zlib、libssh、TinyXML SourceForge 頁、UTCP SourceForge mirror 都可連；原 CodeProject UTCP 頁 HEAD 檢查失敗，先視為脆弱來源。
- 決策：目前不把第三方 tarball 放進 git；只有離線/reproducible release build 需要，或上游 archive 真的不穩時才自養 mirror。UTCP 要先補齊對應 license/source package 才能公開 mirror。

## 2026-07-09 Baseline build

- 新增 `build.bat` 與 `build_scripts.ps1`，本機 baseline 先固定走 Visual Studio x64 Release package，不同時處理 MinGW/Win32/ARM64。
- `.\build.bat -CheckOnly` 會檢查 VS C++ tools、CMake、cl、nmake、Perl；`.\build.bat` 會 configure 並 build `package` target。
- 可編譯環境：
  - PowerShell 7.6.2
  - Visual Studio Community 18.6.11822.322 / Visual Studio 18 2026 generator
  - MSVC 19.51.36246，toolset path `VC\Tools\MSVC\14.51.36231`
  - Windows SDK 10.0.26100.0
  - MSBuild 18.6.3+84d3e95b4
  - CMake 4.2.3-msvc3 from Visual Studio
  - Python 3.12.0 at `C:\Python312_x64\python.exe`
  - Strawberry Perl 5.42.2.1 portable under `_build\tools`, downloaded and SHA256-checked by `build_scripts.ps1`
- 踩雷：Git for Windows 的 Perl 5.42.2 缺 `Locale::Maketext::Simple`，OpenSSL `perl Configure` 會失敗；不能只檢查有 `perl.exe`。
- 踩雷：Strawberry Perl portable 的 `c\bin` 內也有 CMake 3.29.2，若放在 PATH 前面會搶走 VS CMake；腳本只把 `perl\bin` 放前面。
- 第一次 `.\build.bat` 成功：OpenSSL 4.0.1、zlib 1.3.2、libssh 0.12.0 都由 `build_3rdparty.py` 下載並顯示 `checksum OK`。
- 產物：
  - `_build\Release\NppFTP.dll`
  - `_build\NppFTP-0.30.22-win64.zip`
- 目前只有 warnings，沒有 build error。主要 warnings 是 UTCP 檔案在 code page 950 下出現 C4819、size/type conversion、OpenSSL 3+ deprecated DES、舊 Winsock API。依規則不要為了消 warning 任意轉檔案編碼。
- 第一次 OpenSSL `nmake` 很慢，會連 test/app/fuzz 目標也編；目前先不改 `build_3rdparty.py`，除非之後真的需要縮短 clean build 時間。

## 2026-07-09 FTPS hostname verification

- 修第一個 security finding：FTPS 憑證驗證原本只看 chain/手動接受憑證，沒有把 server certificate 綁到 profile hostname。
- 新增 `src/FTPSHostnameVerifier.h`，用 OpenSSL `X509_check_host` 檢查 DNS hostname，並用 `X509_check_ip_asc` 支援 IP SAN。
- `FtpSSLWrapper` 現在保存 expected hostname；`FTPClientWrapperSSL` 建立時把 profile hostname 傳入。`OnSSLCertificate` 只有在 chain valid 或既有例外命中，且 hostname match 時才靜默接受。
- CA-valid wrong-host certificate 現在會進入既有 FTPS certificate warning flow，不再直接視為 valid。使用者當次仍可手動接受；永久例外仍未 host/profile scoped，留給下一個 TODO。
- 新增 `tests/ftps_hostname_check.cpp` 作為最小 regression check，驗證 DNS SAN/IP SAN 的 match 與 mismatch。
- 驗證：
  - 先編譯測試時 helper header 不存在，`tests\ftps_hostname_check.cpp(1): fatal error C1083`，確認紅燈有效。
  - 補 helper 後，用 MSVC 編譯並執行 `_build\tests\ftps_hostname_check.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有既有 warnings，沒有 error。

## 2026-07-09 FTPS certificate exception scope

- 修第二個 security finding：使用者接受過的 FTPS invalid certificate 以前只用 DER 全域記錄，任何 profile/host/port/security mode 都可能重用同一張例外。
- 新增 `src/FTPSCertificateScope.h`，scope 必須同時符合 `profileName`、`profileParent`、`hostname`、`port`、`securityMode`。
- `SSLCertificates` 新增 scoped X509 store，`Certificates.xml` 新格式會把 DER 與 scope 欄位一起保存；舊的無 scope `Certificate DER="..."` 條目不再自動載入成 trusted exception，使用者需要在正確 profile/host 下重新接受。
- `FtpSSLWrapper::OnLoadCertificates` 現在只把同 scope 的例外加入 OpenSSL trust store；`OnSSLCertificate` 判斷 previously accepted 時也同樣檢查 scope 與 hostname。
- `FTPProfile::CreateWrapper` 會把 profile name/parent 傳給 FTPS wrapper；clone transfer wrapper 也會保留同一個 scope。
- 新增 `tests/ftps_certificate_scope.cpp` 作為最小 regression check，驗證 profile、parent、host、port、security mode 任一不同都不 match。
- 驗證：
  - 先編譯 scope 測試時 helper header 不存在，`tests\ftps_certificate_scope.cpp(1): fatal error C1083`，確認紅燈有效。
  - 補 helper 後，用 MSVC 編譯並執行 `_build\tests\ftps_certificate_scope.exe`，通過。
  - 重新編譯並執行 `_build\tests\ftps_hostname_check.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有既有 warnings，沒有 error。

## 2026-07-09 Cache path traversal

- 修第三個 security finding：遠端路徑轉成本機 cache path 時，`../` 可經由 `PU::ConcatExternalToLocal` 逃出 cache root。
- 最小修補放在共用入口 `PU::ConcatExternalToLocal`，不在各 caller 分別補：拒絕 external path 裡的 `..` segment，canonicalize cache root 與結果路徑，並確認結果仍位於 root 底下。
- 新增 `tests/cache_path_traversal.cpp`，驗證正常 `folder/file.txt` 可轉換，`../escape.txt` 與 `folder/../../escape.txt` 會被拒絕。
- 驗證：
  - 修補前測試在 `../escape.txt` assert 失敗，確認紅燈有效。
  - 修補後用 MSVC 編譯並執行 `_build\tests\cache_path_traversal.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有既有 warnings，沒有 error。

## 2026-07-09 FTP PASV parsing overflow

- 修第四個 security finding：PASV response 原本用 `strtok`/`atoi` 直接把 server 提供的六段數字組成 IP/port，沒有先限制每段 0..255，巨大數字或異常 token 會走未定義/錯誤路徑。
- 新增 `UTCP/include/ftp_pasv.h`，用 bounded integer parser 解析 `(h1,h2,h3,h4,p1,p2)`；六段都必須是純數字且 <=255，port 不能為 0。
- `UTCP/src/ftp_c.cpp` 四個 PASV 路徑現在共用 `CUT_ParsePASVResponse`，不再各自用 `strtok`/`atoi` 組字串。
- 依使用者指示，`UTCP/src/ftp_c.cpp` 本次改為 UTF-8 no BOM，後續可正常 patch；build 時該檔本身不再噴 C4819，剩下 C4819 來自它 include 的 UTCP header。
- 新增 `tests/pasv_response_parse.cpp`，驗證合法 PASV 可解析，`256`、巨大數字、缺欄位、port 0、非數字 token 都會被拒絕。
- 驗證：
  - 先編譯測試時 helper header 不存在，`tests\pasv_response_parse.cpp(1): fatal error C1083`，確認紅燈有效。
  - 補 helper 後，用 MSVC 編譯並執行 `_build\tests\pasv_response_parse.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有 UTCP header 的 C4819 warnings，沒有 error。

## 2026-07-09 Git SSH key setup

- `origin` 是 `git@github.com:shadowjohn/NppFTP.git`；一開始一般 SSH push 失敗，錯誤是 `Permission denied (publickey)`。
- 已先用 GitHub CLI token 把 `codex/nppftp-security-maintenance` 推上 GitHub。
- 使用者提供的 PuTTY PPK 可用 `plink.exe` 成功通過 GitHub SSH 認證，回應 `Hi shadowjohn!`。
- 本機 repo 已設定 `.git/config` 的 `core.sshCommand` 走 PuTTY `plink.exe` 與該 PPK；這是 local config，不會進 commit。
- 驗證：`git ls-remote origin HEAD` 成功回 HEAD hash。

## 2026-07-09 FTP PASV endpoint policy

- 修第五個 security finding：PASV response 裡 server 回報的 host 不再直接拿來連 data socket，避免被導到非控制連線 peer 的位址。
- `CUT_ParsePASVEndpoint` 仍會解析並驗證 PASV 六段數字與 port，但 data host 一律使用控制連線已連上的 `m_szAddress`。
- `ResumeReceiveFilePASV`、`ReceiveFilePASV`、`SendFilePASV`、`GetDirInfoPASV` 都改走同一個 helper。
- 新增 regression：server 回 `10.0.0.5` 時，data host 仍應是控制 peer `203.0.113.9`。
- 驗證：
  - MSVC 編譯並執行 `_build\tests\pasv_response_parse.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`8A142DA59B833D89FA4AB55622F2948D80DBEE71FDF41690380955DFD0476539`。
  - 仍有既有 UTCP header 的 C4819 warnings，沒有 error。

## 2026-07-09 RemoveCRLF unsigned underflow

- 修第六個 security finding：`CUT_StrMethods::RemoveCRLF` 對空字串會用 `size_t` 做 `len - 1`，造成 unsigned underflow 並讀到 buffer 前方。
- 同檔 `IsWithCRLF` 也有空字串讀 `buf[len-1]` 的同型問題，一併在同一個字串工具邊界補掉。
- `UTCP/src/utstrlst.cpp` 不是 UTF-8，`apply_patch` 無法讀；本次用 byte-for-byte Latin-1 roundtrip 只替換 ASCII 條件式，避免改檔案編碼。
- 新增 `tests/remove_crlf.cpp`，用 guard page 讓舊碼空字串 underflow 穩定重現。
- 驗證：
  - 修補前 `_build\tests\remove_crlf.exe` 回 `-1073741819` access violation，確認紅燈有效。
  - 修補後同一測試回 `remove_crlf_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`EEB862AEF1A59522D28EB98BBFEE17195696EAC7B99FC4E72421C3C7BCF8F614`。
  - 仍有既有 UTCP 編碼 C4819 warnings，沒有 error。

## 2026-07-09 SFTP directory listing path composition overflow

- 修第七個 security finding：SFTP `GetDir` 把 parent path 與 server 回傳的 filename 用 `strcpy`/`strcat` 寫入 `FTPFile::filePath[MAX_PATH+1]`，惡意 server 傳回超長 filename 會 overflow。
- `FTPClientWrapperSSH::GetDir` 三處修補：
  1. **Line 121–125**：`strcpy`+`strcat` 替換為 `snprintf`，組合路徑超出 buffer 時 skip 該 entry 並釋放 sftp_attributes。
  2. **Line 112**：`path[strlen(path)-1]` 對空字串會讀 `path[-1]`；改為先存 `pathlen`，用 `(pathlen > 0)` 守護。
  3. **Line 151**：`strncpy(file.mod, sfile->longname, sizeof(file.mod)-1)` 沒確保 NUL 結尾；補上 `file.mod[sizeof(file.mod)-1] = '\0'`。
- 同型修補 `FTPSession::GetDirectoryHierarchy`（line 274, 296）：`sprintf(currentPath, "%s%s/", currentPath, pathEntry)` 同時是 overlapping source/destination UB 與無 bounds check；改為 `snprintf` in-place append，超長時 early return 並釋放已分配的 `parentDirs`。
- 新增 `src/sftp_dir_path.h`，把 path composition 邏輯抽成可獨立測試的 inline helper。
- 新增 `tests/sftp_dir_path.cpp`，驗證正常路徑、超長 overflow 拒絕、邊界長度、空路徑、NULL 輸入。
- 驗證：
  - MSVC 編譯並執行 `_build\tests\sftp_dir_path.exe`，`sftp_dir_path_exit=0`，通過。
  - `.\build.bat` 通過，只重編 `FTPClientWrapperSSH.cpp` 與 `FTPSession.cpp`，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`6CAF3798B3C64F4D9FB0AB6124D8F8AFD04049D9E378696D3E81A7D15F508C0C`。
  - 仍有既有 UTCP 編碼 C4819 warnings，沒有 error。

## 2026-07-09 Pin GitHub Actions by commit SHA and set explicit workflow permissions

- 將 GitHub Actions 工作流（Workflow）使用的 Actions 全數改為使用完整的 40 字元 commit SHA 進行固定（Pinning），而非使用可變動的 version tags，並在註解中保留原始 Tag（如 `# v7`）以維持可讀性。
- 調整項目包含 `actions/checkout`、`actions/upload-artifact`、`softprops/action-gh-release` 及 `advanced-security/spdx-dependency-submission-action`。
- 安全加固工作流權限（Workflow Permissions）：
  - 於 `CI_build.yml` 頂層設定預設為最小權限 `permissions: contents: read`。
  - `build_windows` 與 `build_linux` 維持 read-only token；tag 發版改由獨立的 `release_artifacts` job 使用 `permissions: contents: write`。
  - `spdx_upload.yml` 維持原有的局部權限宣告並將 actions 釘死在對應的 commit SHA。

## 2026-07-09 Replace default profile password storage with Windows DPAPI or equivalent

- 修正第八個 security finding（預設設定檔密碼儲存弱點）：原本在未使用 Master Password 時，密碼與私鑰 Passphrase 是使用寫死的 DES 金鑰 `"NppFTP00"` 進行加密（等同於明文儲存）。
- 使用 Windows Data Protection API (DPAPI) 進行安全加固：
  - 於 `src/Encryption.cpp` 實作 `DPAPI_encrypt` 與 `DPAPI_decrypt` 輔助函式，內部呼叫 Win32 的 `CryptProtectData` 及 `CryptUnprotectData`。
  - 當沒有啟用 Master Password 且 `IsDefaultKey()` 為真時，新儲存的密碼/Passphrase 會以 DPAPI 加密，並在輸出的十六進位字串前綴 `"DPAPI:"` 識別。
  - 完美相容舊版設定檔（Backward Compatibility）：在解密時，若字串不包含 `"DPAPI:"` 前綴，會自動回退使用 DES 搭配預設金鑰 `"NppFTP00"` 進行解密，確保使用者更新插件後既有密碼不會遺失。
  - DPAPI 加密失敗時不再暗退回預設 DES；`FTPProfile::SaveProfile` 會回傳 `NULL` 讓儲存失敗，而不是寫入弱加密密碼。
  - 當使用者啟用 Master Password 時，系統會依舊自動切換為 DES 搭配 Master Password 加密模式。
- 新增 `tests/encryption_dpapi.cpp` 單元測試：
  - 測試預設模式下的 DPAPI 加解密（產生 `"DPAPI:"` 前綴）。
  - 測試舊版 DES 密碼的向後相容性解密。
  - 測試 Master Password 模式下切換回 DES 加解密之行為。
- 驗證：
  - 編譯並執行 `_build\tests\encryption_dpapi.exe`，`encryption_dpapi_exit=0` 通過所有 Assert 驗證。
  - 執行 `.\build.bat` 成功，產出新的 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`79FBCF76122C1B6BBB70FE3954C3F25CDF10F71B962669C2EEAB199CB8532E20`。

## 2026-07-09 Add caps for FTP multiline responses and directory listings

- 修正第十個 security finding（FTP 協定封包長度未限制弱點）：當連線至惡意或設定錯誤的 FTP 伺服器時，若伺服器發送無限的 FTP 多行回應（Multiline Response）或極大的目錄清單（Directory Listing），原本程式會無限循環讀取並分配記憶體，造成記憶體耗盡（OOM）或 CPU 飆高造成阻斷服務（DoS）攻擊。
- 限制資源消耗的加固措施：
  - 於 `UTCP/src/ftp_c.cpp` 定義最大限制常數：
    - `MAX_FTP_MULTILINE_RESPONSE_LINES = 10000` (控制連線多行回應最大限制 10,000 行)。
    - `MAX_FTP_DIR_INFO_LINES = 100000` (目錄清單檔案數量最大限制 100,000 筆)。
  - 於 `CUT_FTPClient::PeekResponseCode` 迴圈中加入行數計數與限制判定，超出時回傳 protocol failure，避免控制連線殘留資料後仍被當成成功連線。
  - 於 `CUT_FTPClient::GetDirInfo` 及 `CUT_FTPClient::GetDirInfoPASV` 接收目錄封包的迴圈中加入檔案數量計數與限制判定，超出時即關閉連線並回傳 `UTE_LIST_FAILED` 錯誤。
- 新增 `tests/ftp_response_cap.cpp` 單元測試：
  - 啟動一個本地端的 Mock FTP 伺服器執行緒。
  - 模擬伺服器發送 10,050 行的無限多行回應。
  - 驗證用戶端在讀取回應達 10,000 行上限後，連線會失敗關閉，不會把 capped greeting 當成成功。
- 驗證：
  - 編譯並執行 `_build\tests\ftp_response_cap.exe`，`ftp_response_cap_exit=0` 通過所有測試。
  - 執行 `.\build.bat` 成功，產出新的 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`1DA9917471C5DC237969EBFD069C92BE1012D6312AEB1BDB812956BFDE6A54C7`。

## 2026-07-09 Review fixups for external security pass

- 修正 review 發現的四個問題：
  - `CI_build.yml` 不再讓 untrusted build job 持有 `contents: write`；tag-only `release_artifacts` job 下載 artifacts 後再發 GitHub Release。
  - FTP multiline response cap hit 現在 fail closed；`client.Connect` 對超長 greeting 會回錯，不會把殘留 response 留在控制連線後續使用。
  - `FTPClientWrapperSSH::GetDir` 改用 `sftp_compose_entry_path`，避免測試 helper 和產品碼各跑一套。
  - DPAPI 預設加密失敗不再暗退回 DES；測試也移除加密與解密後密碼的 stdout 輸出。
- 額外補 `FTPProfile` 錯誤路徑：passphrase 解密失敗時不再 `strdup(NULL)`；profile password/passphrase 加密失敗時 `SaveProfile` 回傳 `NULL`。
- 額外補 `Encryption.cpp` 邊界處理：DPAPI 空字串 round-trip 有測到，`strlen()` 轉舊介面 `int` 前會先檢查 `INT_MAX`，避免新修補帶進 C4267 類型警告。
- 驗證：
  - `tests\ftp_response_cap.cpp` 修測試後先紅燈：舊碼回 `Connect result: 0` 並 assertion fail。
  - 修補後 `_build\tests\ftp_response_cap.exe` 回 `ftp_response_cap_exit=0`，輸出 `Connect result: 38`、`Multiline line count: 10000`。
  - `_build\tests\sftp_dir_path.exe` 回 `sftp_dir_path_exit=0`。
  - `_build\tests\encryption_dpapi.exe` 回 `encryption_dpapi_exit=0`，涵蓋 DPAPI 空字串、舊 DES 相容、Master Password 模式，且不再輸出密碼內容。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`56D0A89BFB65D4F75F4B47EC208E322BB18C705CC6D1455EDE4371851927047D`。

## 2026-07-09 PSPad-style flat remote browser first slice

- 依照 `snapshot/new.png` 的方向做第一版 current-directory browser：連線後隱藏原本樹狀視圖，改顯示目前 FTP host、目前路徑、quick search、change-dir combo，以及單層 list view。
- 保留舊 tree code 與 `FileObject` 資料流：profiles tree 斷線時照舊使用；連線時 tree 仍在背景接收 `OnDirectoryRefresh`，flat list 只顯示目前目錄 children，避免重接 FTP/SFTP queue。
- 新 flat list 行為：
  - 顯示 `..`、folders、files。
  - quick search 使用 case-insensitive substring filter。
  - change-dir combo 可手動輸入路徑並按 Enter；也會記住最近 8 筆本次執行期目錄。
  - double-click folder 進入該目錄；double-click file 沿用 `DownloadFileCache()` 下載後開檔。
  - toolbar 的 Open Directory 也改走同一條 `NavigateRemotePath()`，讓 flat view 能同步跳轉。
- 本次刻意不做：recent dirs 寫入 profile/settings、list context menu、drag/drop、folder/file icon。先讓單層瀏覽穩定可用。
- 新增 `src/remote_browser_utils.h` 與 `tests/remote_browser_utils.cpp`，只測 quick search filter 行為。
- 驗證：
  - 先跑紅燈：`tests\remote_browser_utils.cpp` 因缺少 `src/remote_browser_utils.h` 編譯失敗。
  - 補 helper 後 `_build\tests\remote_browser_utils.exe` 回 `remote_browser_utils_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`7D268F7846E479A8A11C7C9E05A2DF3E1C800ED97A9DE82CE48279846434F996`。

## 2026-07-09 Open tasks for flat remote browser polish

- 根據 Notepad++ 實測截圖與 PSPad 參考畫面，將下一刀拆成可獨立驗證的 task：resize/layout、icons、metadata columns、header drag/drop、busy cursor、typed path handling、spacing alignment、manual QA。
- 建立 `docs/superpowers/plans/2026-07-09-remote-browser-polish.md`，並同步更新 `todo.md` 的 `4.1 PSPad-style remote browser polish` 清單。
- 決策：先保留 source labels 為 ASCII，避免在這刀混入編碼/i18n 變更；若要中文 labels，另開 resource/i18n task。

## 2026-07-09 Fix flat browser resize layout

- 修正 PSPad-style flat remote browser 在 splitter 拖曳、放開、capture 變更後沒有同步重排的問題；每次 `WindowSplitter` 移動隱藏 tree panel 後，flat controls 會補跑 `LayoutRemoteBrowser()`。
- 補上 dock 高度過小時的 `clientheight` 下限，以及 search/change-dir/list column 的寬度下限，避免縮太小時把負尺寸丟給 `MoveWindow()` 或 list column。
- 決策：這刀不改 `WindowSplitter` 本體，降低影響舊 tree/queue layout 的風險；先把 flat browser 跟上既有 splitter 結果。
- 驗證：
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`5FDE4F9E6E84B87724E09F1D848883FD0EEA6F2E0453B5BA0CD9233AD9D843CF`。
  - Notepad++ 內的 shrink/enlarge/splitter/reconnect 手動 QA 尚未執行，留到完成 polish slice 後一起測。

## 2026-07-09 Add flat browser metadata columns

- 將 flat remote browser 的 list view 改為 PSPad 順序欄位：`Name`、`Size`、`Modified`、`Type`、`Permissions`，並開啟 `LVS_EX_HEADERDRAGDROP` 讓使用者可拖曳欄位順序。
- 資料列現在會從 `FileObject` 填入 metadata：檔案 size、local modified time、檔案 type、副檔名大寫、permissions/mod string；資料夾 size/type 留空。
- folder/file icon 重用既有 `TreeImageList`，新增只讀 `GetImageList()` 讓 flat list view 掛同一組 Windows small icons，避免另養 icon 資源。
- 新增 `remote_browser_format_size()`、`remote_browser_type_text()`、`remote_browser_permission_text()` helper，並補 `tests\remote_browser_utils.cpp` 測試。
- 驗證：
  - 先跑紅燈：`tests\remote_browser_utils.cpp` 因缺少 metadata helper 編譯失敗。
  - 補 helper 後 `_build\tests\remote_browser_utils.exe` 回 `remote_browser_utils_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`FD57618A846554272A141B09DE6F9E9E630D04954571CAA2C2CD4BB2882235D8`。

## 2026-07-09 Show busy cursor for flat browser actions

- 新增 `m_remoteBusyCursor`，flat browser 進入資料夾、輸入路徑觸發 directory hierarchy、或開啟檔案下載快取時，會先把 remote controls 的游標切成 `IDC_WAIT`。
- `WM_SETCURSOR` 只在 remote labels/search/combo/list 及其 child control 上套用 wait cursor，splitter 游標仍優先。
- directory get / download queue operation 結束、connect/disconnect 收尾時會清掉 busy flag，避免游標卡住。
- 驗證：
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`0E9A2F883BD483CDDA09E4DABD5D3B5E72DD259C524DBDB21D44603A55B562E0`。
  - Win32 cursor 行為尚待 Notepad++ 內手動 QA 確認。

## 2026-07-09 Handle typed flat browser paths

- `Change dir` combo 的 Enter 現在只對已知 cache tree 物件動作：已知 directory 會切換目前資料夾，已知 file 會走 `DownloadFileCache()` 開啟，未知路徑直接 no-op，不 beep、不排 `GetDirectoryHierarchy()`。
- 下拉選取 recent dir 仍會導航；移除 combo `CBN_KILLFOCUS` 自動導航，避免輸入框失焦時意外開檔。
- 新增 `remote_browser_simplify_typed_path()`，支援絕對路徑、相對 child、`../sibling`，輸出超過 buffer 時回失敗。
- 驗證：
  - 先跑紅燈：`tests\remote_browser_utils.cpp` 因缺少 `remote_browser_simplify_typed_path()` 編譯失敗。
  - 補 helper 後 `_build\tests\remote_browser_utils.exe` 回 `remote_browser_utils_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`E457E516B5455CA9B8BFD69A471E75B316135157D6F671555B32710031C7A31F`。

## 2026-07-09 Align flat browser labels

- 微調 flat remote browser 的 layout 常數：左邊距、label 欄寬、最小寬度、row spacing 與 list 底部留白，讓 FTP line、current path、Quick search、Change dir、list 更接近 PSPad 參考畫面。
- 本刀刻意維持 source labels 為 ASCII：`FTP:`、`Quick search:`、`Change dir:`、`Name` 等文字未轉中文，避免把 UI spacing 與 source encoding/i18n 混在同一筆。
- 驗證：
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`3CE1DD97D728532F2E0745075E96DD0D6AC1B2B31E6419776717A3C0523D2680`。

## 2026-07-09 Update README maintenance notes

- 更新 `README.md`，補上此 fork 目前維護重點：安全加固、可重現 Windows build、PSPad-style flat remote browser，以及相關文件入口。
- 決策：README 維持 ASCII 文字，避免在原本簡短英文文件中混入編碼/i18n 變更；詳細中文歷程仍放 `history.md`。
- 驗證：文件-only 變更，未重跑 build；執行 `git diff --check` 無 whitespace error。

## 2026-07-09 Promote maintenance branch to master

- 將 `codex/nppftp-security-maintenance` 以 fast-forward merge 簽進 `master`，master 從 `8b0e0e1` 更新到 `4cfb9b7`。
- 合併前驗證：`_build\tests\remote_browser_utils.exe` 回 `remote_browser_utils_exit=0`，且 `.\build.bat` 通過。

## 2026-07-09 Add README snapshot comparison

- 將 `snapshot/orin.png` 與 `snapshot/new.png` 納入版本控制，作為 README 的舊版 tree browser 與新版 PSPad-like flat browser 對照圖。
- `README.md` 改成中文詳細版，補上設計理由、安全維修摘要、build 方式、專案紀錄入口與剩餘手動 QA。
- 驗證：文件與圖片-only 變更，未重跑 build；執行 `git diff --check` 無 whitespace error。

## 2026-07-09 Add maintained GitHub Actions build

- 在既有 `.github/workflows/CI_build.yml` 加入 `maintained_windows_x64` job，push / pull request / manual dispatch 時會直接執行 `build.bat -Arch x64 -Config Release`。
- 上傳 `_build\NppFTP-0.30.22-win64.zip` 與 `_build\Release\NppFTP.dll` 為 `NppFTP-maintained-win64` artifact。
- 決策：先保留既有 upstream Windows/Linux matrix，不在這刀刪平台；新增 job 只覆蓋目前本 fork 實際維護的 Windows x64 Release build path。
- 驗證：`.\build.bat -CheckOnly` 通過，`CI_build.yml` 可被 Python YAML parser 讀取，`git diff --check` 無 whitespace error。

## 2026-07-09 Refresh README screenshots

- 更新 README 圖片對照：`snapshot/pspad.png` 作為 PSPad FTP 參考圖，`snapshot/new_notepad.png` 作為新版 NppFTP 實作畫面。
- 移除舊的 `snapshot/new.png`，README 現在清楚分成舊版 NppFTP tree、PSPad 參考、新版 NppFTP flat browser 三段。
- 驗證：文件與圖片-only 變更，未重跑 build；執行 `git diff --check` 無 whitespace error。

## 2026-07-10 Keep only maintained Windows build action

- 移除 `CI_build.yml` 內舊 upstream Windows/Linux matrix 與 tag release artifact job，只保留 `maintained_windows_x64`。
- 原因：GitHub Actions run `29007714842` 中 `maintained_windows_x64` 已成功產出 artifact，但舊 matrix 的 Win32 / Linux CMake 路線因 zlib 下載 checksum mismatch 拖紅整個 workflow。
- 決策：P0 先只保證 `v0.30.22-3wa.1` 需要的 Windows x64 Release build；舊平台不要在第一版 release 前搶修。

## 2026-07-10 Design flat remote file operations

- 核准將 flat remote browser 補成實用的檔案操作面：目錄、檔案、空白區各自有精簡右鍵選單；`F2` 更名；CHMOD 改用 Owner / Group / Other 九格權限勾選與三位八進位值同步的原生對話框。
- 上傳目標規則：拖到空白或檔案就傳到目前目錄，拖到目錄就傳進該目錄；檔案選擇視窗可多選。
- 單檔覆蓋依目標目錄目前的 flat-list cache 判斷；`Skip` 只略過該檔，`Don't ask again this session (overwrite all)` 僅在按 Overwrite 時生效，斷線後重設。刻意不為單檔上傳增加遠端 preflight。
- 本機目錄可遞迴上傳；遠端同名目錄採合併，不刪除遠端內容重建。因遞迴子目錄需要在上傳前找出同名檔，目錄上傳會做受限的遠端掃描，再依既有 queue 順序先建父目錄、後傳檔案，讓 queue 顯示每個檔案的進度。
- 失敗提示：單一 CHMOD / 更名 / 刪除 / 建立 / 上傳失敗要跳出結果提示並記到 Output；遞迴上傳逐筆寫 Output，結束只跳一個摘要。SFTP 可顯示權限不足或檔案不存在；FTP/FTPS 若只有模糊 server rejection（常見 550），必須如實顯示不確定原因，不能硬猜。
- 下一刀另做多國語系選擇，預設正體中文；本刀新增 UI 文字先維持英文，避免把 i18n 與檔案操作風險混在同一組變更。
- 詳細規格：`docs/superpowers/specs/2026-07-10-remote-file-operations-design.md`。
- 實作拆成兩份可獨立驗收的 task 計畫：先做直接檔案操作與錯誤提示，再做遞迴資料夾掃描、合併上傳與批次摘要；文件位於 `docs/superpowers/plans/2026-07-10-flat-remote-file-actions.md` 與 `docs/superpowers/plans/2026-07-10-recursive-directory-upload.md`。

## 2026-07-10 Add graphical CHMOD dialog

- 將 remote browser 原本的純數字 CHMOD 輸入框改成 Win32 原生對話框，Owner / Group / Other 各有 Read / Write / Execute 三個勾選框，並即時顯示三位八進位模式。
- 權限字串與八進位模式的轉換集中在 `remote_browser_utils.h` 的三個小 helper；不新增 UI framework 或額外 dependency。
- TDD：先加入 `755`、`640`、NULL 與非法八進位輸入 assertions，確認因 helper 不存在而編譯紅燈；補最小實作後 `_build\tests\remote_browser_utils.exe` 輸出 `remote_browser_utils_exit=0`。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`3995AAECCA88B6882F29D07854CE94E933DD8729FB4BD4768F18FB5F14E98A47`。
- CHMOD 對話框視覺與實際 FTP/SFTP 權限變更仍待 Notepad++ 實機 QA。

## 2026-07-10 Add flat remote file actions

- flat remote list 新增三組英文右鍵選單：檔案可 Edit / CHMOD / Delete / Rename，目錄可 Upload files / CHMOD / Delete / Rename，空白區可 Refresh / Create directory / New file / Upload files；`..` 不顯示破壞性選單。
- 新增 `F2` 更名與 Win32 多檔選擇視窗。拖放規則為：空白或檔案落在目前目錄，目錄列落在該目錄；flat list 與舊 tree 共用既有 OLE drop target，並改用 `ReleaseStgMedium` 正確釋放資料物件。
- 新增檔案衝突對話框：`Skip` 只略過目前檔案、`Cancel` 停止剩餘批次；只有按下 `Overwrite` 並勾選 overwrite-all 才會記住本次連線，斷線即重設。
- 本機資料夾已能在 picker/drop 路徑中被辨認，但本刀只寫入 Output 並略過，不會誤傳給單檔 `UploadFile`；遞迴建立與上傳依後續計畫實作。
- TDD：先加入 overwrite decision assertions 並確認 undeclared enum 的編譯紅燈；補最小 enum 後 `_build\tests\remote_browser_utils.exe` 輸出 `remote_browser_utils_exit=0`。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`DC2239025B3D9CF88E230F81F6BCBDC34CC55B56CC2FFB382F298284AFCE596E`。
- 三組右鍵選單、F2、多檔 picker、三種 drop target、Skip / Cancel 與斷線重設 overwrite-all 仍待 Notepad++ 實機 QA。

## 2026-07-10 Report remote operation failures

- 新增 `RemoteFailureKind`，只分 `Unknown`、`PermissionDenied`、`NotFound`；FTP/FTPS base wrapper 固定回 Unknown，避免把模糊 server rejection 誤判成權限問題。
- SFTP 只依 libssh 的 `SSH_FX_PERMISSION_DENIED` 與 `SSH_FX_NO_SUCH_FILE` 分類，其餘仍回 Unknown；同步將 SSH/SFTP session 指標在 constructor 初始化為 `NULL`。
- `QueueOperation` 在 end notification 尚未 ACK 前，從實際執行的 wrapper 讀取 failure kind。上傳、建立/刪除目錄、建立/刪除檔案、更名與 CHMOD 失敗時保留原有 Output 訊息，另顯示固定英文提示；成功不顯示 modal。
- TDD：先 include 尚不存在的 `RemoteFailure.h`，確認 focused test 以 C1083 紅燈；補 enum 與最小傳遞鏈後 `_build\tests\remote_browser_utils.exe` 輸出 `remote_browser_utils_exit=0`。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`C49F7D3632B6E49F85C76B374FBA2868F667C76453123768DF285B72AE55C2C1`。
- SFTP 權限不足、SFTP 路徑不存在，以及 FTP/FTPS generic rejection 的實際 modal 仍待 Notepad++ 實機 QA。

## 2026-07-10 Add remote directory upload plan

- 新增 owned `RemoteUploadPlan` / `RemoteUploadItem`，以 Win32 `FindFirstFile` / `FindNextFile` 枚舉本機目錄，將本機名稱轉成 UTF-8 remote path，所有 local/remote path 都限制在 `MAX_PATH` 內。
- `AddDirectory` 永遠插在第一個檔案前，因此即使檔案系統枚舉順序不同，結果仍維持父目錄、子目錄、檔案的 queue 前置順序。
- 安全邊界：root 或子項目只要是 `FILE_ATTRIBUTE_REPARSE_POINT` 就不跟隨，避免 junction / symlink 走出使用者拖入的目錄樹或形成循環。
- `ApplyRemoteDirectoryListing` 只將同一 remote parent 下的同名非目錄項目標成 collision；同名 remote directory 不會被當成檔案覆蓋候選。
- TDD：先確認缺少 `RemoteUploadPlan.h/.cpp` 的 C1083 紅燈；補實作後，手動順序、synthetic `FTPFile` collision 與 `_build\tests` 巢狀 fixture 均通過，輸出 `remote_upload_plan_exit=0`。另修正原計畫不能對多 source 共用單一 `/Fo...obj` 的 MSVC 指令。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`DBC2AF958831E0FA98187F6DF874B9EB10F5F5327B1DBA9F9BEAAEFA0CF11F71`。

## 2026-07-10 Queue recursive remote uploads in order

- 新增 remote upload scan、ensure-directory、batch upload 與 complete marker operations；scan 只走 `m_mainQueue`，所有 mkdir / upload / complete 嚴格依 planner 順序放進同一條 `m_transferQueue`，避免跨 queue 建目錄競賽。
- scan 只在 wrapper 明確回 `RemoteFailureNotFound` 時繼續；PermissionDenied 與 Unknown 都停止。FTP/FTPS 常把不存在與權限問題都包成模糊 550，因此本刀選擇不猜、不冒險繞過 collision scan，實際相容性留待 real-server QA。
- ensure-directory 先嘗試 `MkDir`；若失敗但 `GetDir` 成功，就視為已存在目錄並安全合併，不刪除遠端內容，也不把同名檔誤認為目錄。
- `RemoteUploadBatch` 擁有 plan、refresh target、完成數與 failures。recursive mkdir / upload / complete operation 各持 interlocked reference，避免使用者在 active transfer 中斷線、`ClearQueue` 先刪 complete marker 後造成 notifyData use-after-free。
- focused test 新增 selected 狀態與 batch refcount smoke checks，`_build\tests\remote_upload_plan.exe` 輸出 `remote_upload_plan_exit=0`；enqueue 順序由已測 planner vector 與單一 transfer-queue loop 保證，未留下暫時 diagnostic。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`F78F2F490465C0F8E6F7CC91C775D5D563030094D561F70960230E870CACEEF0`。
- 本筆只完成 queue/session 層，flat browser 的 directory picker/drop 接線、collision prompt、batch summary 與每列 progress 留在下一筆。

## 2026-07-10 Upload remote directories recursively

- flat browser 的 OLE drop 現在接受檔案與本機目錄混合輸入；空白/檔案列仍以上傳到目前 remote directory，目錄列以上傳到該目錄。本機目錄先建立 owned plan，再進 remote preflight 與 ordered transfer queue。
- preflight 先 LIST 使用者選定且已存在的 target，標出 local root/child 哪些 remote directory 真的存在，只深入掃描存在者。新目錄不需要猜 FTP/FTPS 模糊 550 的意思，也能安全交給 mkdir；若已存在目錄本身無法 LIST，仍停止而不繞過 collision scan。
- remote 同名 directory 採 merge，不刪除重建；remote link 對 local directory 視為需深入 scan，對 local file 視為 collision，避免沿未掃描 symlink 直接寫入。
- remote scan 成功後在 UI thread 逐一顯示既有 overwrite dialog；Skip 只取消該檔，Cancel 取消該 local directory，overwrite-all 沿用本次連線狀態並於 disconnect 重設。
- batch mkdir/upload 失敗逐筆寫 Output，不跳單檔 modal；complete marker 只 refresh 選定 target 一次，並只顯示一個完成或失敗數摘要。`QueueWindow::ProgressQueueItem` 依 operation pointer 更新任意列，不再只允許 index 0。
- focused test 覆蓋 parent-before-child path、selected、target-first directory detection、file/directory/link collision、fixture enumeration 與 batch refcount，輸出 `remote_upload_plan_exit=0`。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`D4D44E738558D82A76F42E180172AF71ABDF42730004CA9C46C5F6BDA9CFEA5A`。
- 尚未替使用者連線執行 remote mutation；FTP/SFTP 新/舊目錄 merge、nested collision、symlink、每列 progress 與單一 summary 仍待 Notepad++ 實機 QA。

## 2026-07-10 Fix F2 remote rename routing

- 根因：Notepad++ 會先對已登記的 modeless 視窗執行 `IsDialogMessage`，再處理全域 accelerator；NppFTP dock 視窗未登記，因此 F2 被 Notepad++ 的快捷鍵先吃掉，flat list 的 `LVN_KEYDOWN` 不會觸發。
- 修正：建立/銷毀 FTPWindow 時以 `NPPM_MODELESSDIALOG` 登記/解除登記；remote list subclass 只在 F2 的 `WM_GETDLGCODE` 加上 `DLGC_WANTALLKEYS`，其餘鍵盤訊息維持原行為，並沿用既有 `PromptRemoteRename` handler。
- TDD：新增 Win32 focused test，以真實 `IsDialogMessage` 驗證 F2 可抵達 list view 的 `LVN_KEYDOWN`；先因 `remote_browser_wants_key` 不存在而紅燈，補最小實作後輸出 `remote_list_keyboard_exit=0`。
- `.\build.bat -Arch x64 -Config Release` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；ZIP SHA256：`9E79B7CF81D9272F7AD051EFD4FBEE12C2F74F5B24E35CB4EEFFC49A402A0F81`。F2 仍待 Notepad++ 實機複驗。

## 2026-07-10 Add local Notepad++ deployment helper

- 新增 `copyNppFTPdllToRealENV.bat`，將 repo 相對路徑 `_build\Release\NppFTP.dll` 覆蓋到 `C:\Program Files\Notepad++\plugins\NppFTP\NppFTP.dll`。
- 腳本先確認 build output 與正式 plugin 目錄存在，使用 `copy /Y /B` 覆蓋，再以 `fc /B` 驗證內容一致；失敗時提示關閉 Notepad++ 並以系統管理員身分執行。
- 腳本不自動要求 UAC、不建立缺少的正式目錄，也未由自動驗證直接修改目前安裝中的 plugin。

## 2026-07-13 Fix local TortoiseGit pull SSH mismatch

- 根因：repo-local `core.sshCommand` 指向標準 PuTTY `plink.exe`，但 TortoiseGit 2.19.1 對自己的 `TortoiseGitPlink.exe` 環境設定 `GIT_SSH_VARIANT=ssh`；環境 variant 優先，Git 因而把 OpenSSH 的 `-o` 參數傳給不支援它的標準 plink。
- repo-local SSH command 改用 `TortoiseGitPlink.exe`，保留原有 PPK 參數，不修改 global Git 設定，也不把個人金鑰路徑寫入追蹤檔。
- 驗證：標準 PuTTY plink 對 `-o` 回 `unknown option`，TortoiseGitPlink 可接受；以 TortoiseGit 同等 `GIT_SSH` / `GIT_SSH_VARIANT=ssh` 環境執行原 pull command 成功，回 `Already up to date`、exit 0。

## 2026-07-17 Activate focused remote items with Enter

- flat remote list 原本已有 `NM_RETURN` 與 double-click 共用的 `ActivateRemoteListSelection`，但 modeless key gate 只放行 F2，Enter 會先被 `IsDialogMessage` 吃掉。
- 最小修正是在既有 `remote_browser_wants_key` 同時放行 `VK_RETURN`；目錄仍走 `SetRemoteCurrentDir`，檔案仍走 cache download/open，不新增第二套 activation 邏輯。
- TDD：focused Win32 test 建立並選取真實 ListView item；修正前 Enter 無法產生 `NM_RETURN`，修正後輸出 `remote_list_keyboard_exit=0`。
- `.\build.bat -Arch x64 -Config Release` 通過；ZIP SHA256：`8443B07043388B7E2CD5EAB5D249AA772651D51E3EED8AA4DE3B0253FE9470BB`。Notepad++ 實機仍待複驗。

## 2026-07-17 Refresh README status and remaining work

- README 補上 Enter activation、flat-list 右鍵操作、CHMOD、拖放與 recursive upload、失敗提示、GitHub pre-release workflow，以及 `copyNppFTPdllToRealENV.bat` 安裝方式。
- 將剩餘項目明確分成「尚未開發」與「已開發但待實機 QA」：目前主線未開發只剩 recent dirs 依 profile 持久化、mutation refresh 後恢復 focus、UI 語系選擇。
- 移除指向 upstream 的 AppVeyor / release badges，改用本 fork 的 maintained Windows x64 workflow 與 pre-release badges。
- 文件-only 變更，未重跑 build；執行 `git diff --check` 驗證格式。

## 2026-07-17 Restore flat-list focus after a mutation refresh

- rename、CHMOD、new file 排入既有的後續 directory refresh 時，記住目標 remote path 與 parent；該 directory 資料重建後，會選取目標列、設為鍵盤 focus，並捲回可見範圍。
- 伺服器回報 mutation 失敗、使用者切換到其他目錄、或 disconnect 時，會清掉待回復狀態，避免舊操作搶走目前畫面焦點。
- TDD：先讓真實 Win32 ListView test 因缺少 `remote_browser_focus_list_item()` 編譯紅燈；補最小 native helper 後 `_build\\tests\\remote_list_keyboard.exe` 輸出 `remote_list_keyboard_exit=0`。
- `_build\\tests\\remote_browser_utils.exe` 同樣通過；`.\\build.bat -Arch x64 -Config Release` 通過，ZIP SHA256：`EEF498613274C9947D4891A45321F4B8F33E515A390B58801B2174DB79F61058`。

## 2026-07-17 Persist profile recent directories with prefix suggestions

- 每個 `FTPProfile` 現在保存最多 8 筆 remote directory；重複路徑會移到最前方，不新增重複項。資料寫入既有 `NppFTP.xml` 的 profile `<RecentDirs>`，空列表不輸出，舊設定檔可直接讀取。
- Change dir 改為只在實際成功切到目前 remote directory 後才記錄，未知/失敗路徑不會污染 recent；連線時載入該 profile 的清單，手打時以輸入前綴過濾並自動展開原生 ComboBox 下拉。
- TDD：`tests\\recent_dirs.cpp` 先因缺少 recent helper 編譯紅燈，再驗證 move-to-front、去重、8 筆上限與 prefix match；`_build\\tests\\recent_dirs.exe` 輸出 `recent_dirs_exit=0`。
- `.\\build.bat -Arch x64 -Config Release` 通過，ZIP SHA256：`899CB2C896BDBE897165ABEA4A6411E2570D6D9D34814B1F51BB69DE49235A6E`。profile 切換、重啟後資料與 ComboBox 實際互動仍待 Notepad++ 實機 QA。

## 2026-07-20 Plan user-directed remote downloads

- 將 `Download` 與 `Edit` 明確分工：Download 一律由使用者選擇本機目的地，Edit 才使用 cache 並在 Notepad++ 開啟；單檔用 Save As，目錄選父目錄後建立同名 local root 遞迴下載。
- 採用與既有 recursive upload 對稱的 scan + plan + transfer batch；下載前先完整列出遠端子樹、檢查本機碰撞，remote symlink 不遞迴，並保護所有 local output path 必須留在選定 root 內。
- 詳細設計見 `docs/superpowers/specs/2026-07-20-remote-download-design.md`；尚未開始功能實作。

## 2026-07-20 Queue recursive remote downloads

- 新增 remote download scan、file queue、complete marker 與 ref-counted batch；scan 在 main queue 完整列出 selected non-link directories，transfer queue 只排 selected non-link files。
- batch 會保留 plan 及 pre-scan failure，讓下一刀 UI 可在下載完成時統一顯示結果；每個 transfer operation 與 complete marker 各自持有 reference，保護 queue 清除時的生命週期。
- TDD：先以 `RemoteDownloadBatch` 尚未宣告確認 focused test 編譯紅燈；完成後 `_build\\tests\\remote_download_plan.exe` 輸出 `remote_download_plan_exit=0`，`build.bat -Arch x64 -Config Release` 通過。

## 2026-07-20 Route downloads to local Save As or folder selection

- flat remote file/directory menus、legacy Download/Save file as command 與 toolbar Download 現在共用使用者選擇目的地的下載流程；檔案走 Windows Save As，目錄選父目錄後遞迴下載。
- `Edit`、Enter 與 double-click 仍使用 cache download 後在 Notepad++ 開啟；使用者選擇儲存位置的單檔下載完成後只寫 Output，不再詢問或開啟檔案。
- download batch 的碰撞對話框可對 Overwrite 或 Skip 套用至其餘檔案；目錄掃描、local preparation、逐檔失敗會在完成時以單一摘要及 Output 詳情呈現。
- 驗證：加入 selected Skip-path assertion 後重新編譯 focused test，輸出 `remote_download_plan_exit=0`；`build.bat -Arch x64 -Config Release` 通過並產出 DLL 與 ZIP。

## 2026-07-20 Document user-directed remote download workflow

- README 說明 `Download...` 與 `Edit` 的明確分工：單檔 Download 使用 Windows Save As；目錄 Download 選本機父目錄，於其下建立同名 root 後遞迴下載；使用者指定的下載完成後不會開啟 Notepad++。遠端 symlink 一律不遞迴。
- `todo.md` 將已完成的下載實作項目標記完成，並保留 release 前未勾選的完整 manual QA 清單。
- 驗證命令與結果：`& .\_build\tests\remote_download_plan.exe` -> `remote_download_plan_exit=0`；`& .\_build\tests\remote_browser_utils.exe` -> `remote_browser_utils_exit=0`；`& .\_build\tests\remote_list_keyboard.exe` -> `remote_list_keyboard_exit=0`；`.\build.bat -Arch x64 -Config Release` -> `NppFTP.vcxproj -> D:\mytools\NppFTP\_build\Release\NppFTP.dll` 且 `NppFTP-0.30.22-win64.zip generated.`；`git diff --check` 無 whitespace error。
- `Get-FileHash .\_build\NppFTP-0.30.22-win64.zip -Algorithm SHA256`：`FD8CF21084DA8D101641748B99903EE42579F8363AFD634AFB16397BB01A0BD9`。
- 尚未執行真實 FTP / FTPS / SFTP server QA；仍需驗證 Save As、資料夾遞迴、碰撞選擇、scan failure、queue progress 與完成摘要。

## 2026-07-20 Harden remote download conflicts

- `Download...` 在進入檔案或目錄流程前直接拒絕 remote symbolic link，寫入 Output 並提示使用者；`Edit` 維持既有 cache-open 行為。
- planner 現在會取消本機目錄撞到預定檔案目的地的項目，並在同一份 remote listing 出現大小寫不同但映射到相同 Windows 路徑時取消後者，避免兩次傳輸寫入同一目的地。
- focused `remote_download_plan` test 以 `/Fo:_build\tests\` 編譯並輸出 `remote_download_plan_exit=0`，`git diff --check` 通過，`build.bat -Arch x64 -Config Release` 成功產出 DLL 與 ZIP；仍待真實 FTP / FTPS / SFTP 的下載流程 QA。

## 2026-07-20 Harden Unicode local download collisions

- planner 的 local path case-insensitive 比較改用 Windows `CompareStringOrdinal(..., TRUE)`；非 Unicode `TCHAR` build 先轉成 UTF-16，避免依賴 CRT locale 比較而漏掉 Windows 上會碰撞的 Unicode 名稱。
- focused test 以 UTF-8 byte escape 餵入 `Ä.txt` 與 `ä.txt`，確認後者會取消選取並記錄 failure；以 `/Fo:_build\tests\` 編譯後輸出 `remote_download_plan_exit=0`，`git diff --check` 與 `build.bat -Arch x64 -Config Release` 皆通過。

## 2026-07-20 Keep ANSI download paths consistent with UTF-8

- `Utf8SegmentToLocal` 一律先以 `CP_UTF8` 和 `MB_ERR_INVALID_CHARS` 解碼。Unicode build 直接保存 UTF-16；ANSI build 再以 `CP_ACP`、`WC_NO_BEST_FIT_CHARS` 編碼，若需要 default character 或無法編碼就跳過該 remote name 並記錄 failure，絕不把 raw UTF-8 bytes 當成 ANSI local path。
- `remote_download_plan` 分別以 `/DUNICODE /D_UNICODE` 與 ANSI `TCHAR` 編譯，兩者皆使用 `/Fo:_build\tests\` 並輸出 `remote_download_plan_exit=0`；`git diff --check` 與 `build.bat -Arch x64 -Config Release` 也皆通過。

## 2026-07-20 Report canceled directory download items

- queue 現在只在尚未執行的 operation 被 Clear、單筆取消或 shutdown cleanup 移除前呼叫取消 hook；正常完成後的 QueueEventRemove 與正在 abort 的 operation 都不會記為取消。
- remote directory download batch 分開記錄 failed 與 canceled remote path；完成摘要與 Output 使用 finished，列出 successful、failed、canceled 三個數量，只有 failed 與 canceled 都是零時才顯示成功訊息。
- focused planner test 覆蓋 batch cancellation recording；Unicode/ANSI `remote_download_plan` 皆輸出 `remote_download_plan_exit=0`，`git diff --check` 通過，`build.bat -Arch x64 -Config Release` 成功產出 DLL 與 ZIP。真實 FTP / FTPS / SFTP 的取消、摘要與 queue UI 行為仍待手動 QA。

## 2026-07-20 Preserve canceled download summaries after Clear Queue

- Clear Queue 現在會取消並移除所有尚未執行的普通 operation，但保留 `QueueRemoteDownloadComplete` marker；正在執行的 operation 先放回 queue，接著依原始順序接回 marker，讓 active transfer 完成後再顯示 batch 的 failed/canceled 摘要。沒有 active transfer 時也會重新喚醒 queue 執行 marker。
- `CancelQueueOp` 對單一 marker 的既有行為不變，不會提前顯示摘要；focused batch test 保持確認同一 batch 的 completed、failed、canceled 三種狀態分離。Unicode/ANSI `remote_download_plan` 皆輸出 `remote_download_plan_exit=0`，`git diff --check` 通過，x64 Release build 成功產出 DLL 與 ZIP。

## 2026-07-20 Fix queued operation cancellation lookup

- `CancelQueueOp` 改以現有 lock 內的 queue iterator 逐筆比較，命中時只 erase 該 operation，再依既有順序呼叫 `OnQueueCanceled`、通知移除並 delete；不再重複拿 front 卻刪除其他索引。
- running front operation 仍回傳 `-1`、找不到的 operation 不修改 queue，且不改動 Clear Queue 對 download completion marker 的保留邏輯。
- Unicode/ANSI `remote_download_plan` 回歸、`git diff --check` 與 x64 Release build 通過；尚待真實 queue 驗證 active operation 後的單筆 Remove operation 是否正確取消目標並在摘要列為 canceled。

## 2026-07-20 Final remote download verification

- 最終整體 code review 通過；Download/Edit 分流、遞迴掃描、link/path/Unicode 防護、碰撞處理、單筆取消與 Clear Queue 摘要的跨模組行為一致。
- 主工作區重新驗證：`git diff --check` 無錯誤；`remote_download_plan_exit=0`、`remote_browser_utils_exit=0`、`remote_list_keyboard_exit=0`；`build.bat -Arch x64 -Config Release` 成功產出 DLL 與 ZIP。
- 最終 `NppFTP-0.30.22-win64.zip` SHA-256：`32241923D36EB8E63CEDA7F2CDEC65CE45074107ACD8EC1BE97C7E5908815A88`。
- 真實 FTP / FTPS / SFTP QA 仍待執行：Save As、遞迴資料夾、碰撞選項、scan/transfer failure、進度與摘要、active transfer 後單筆取消，以及 Clear Queue 後摘要。

## 2026-07-20 Manual QA: recursive directory download

- 使用者已於 Notepad++ 實機確認遠端整個目錄可成功遞迴下載到選定本機位置。
- 碰撞選項、取消／Clear Queue 摘要，以及 FTP / FTPS / SFTP 的其餘實機情境仍維持待驗證。

## 2026-07-20 Stabilize CI zlib source delivery

- GitHub Actions 兩次從 `zlib.net/fossils/zlib-1.3.2.tar.gz` 收到 SHA-256 不符的內容，build 在驗證階段正確停止；本機以同一個 Python `urllib` 與官方 current URL 驗證，官方 tarball hash 仍為 `bb329a...f0119d16`。
- 改用 zlib 官方 GitHub repository `madler/zlib` 的固定 `v1.3.2` tag archive，tag commit 為 `216c70c020aa53f0c40920d155f808b6b59c9acb`，SHA-256 固定為 `b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1`；建置前仍會驗證 hash，不會放寬供應鏈檢查。

## 2026-07-20 Add third-party source verifier

- 新增 `verify_third_party_sources.py`，會重用 `build_3rdparty.py` 的 URL 與 SHA-256，直接串流下載三個 archive 驗證；不寫入 tarball，任一來源失敗會回傳非零 exit code。
- GitHub Actions `CI_build` run 29731266380 已成功完成 maintained Windows x64 建置與 DLL 封裝，確認新的 zlib tag archive 可穩定通過既有 checksum gate。

## 2026-07-30 Copy current remote path

- flat remote browser 的目前路徑改為可點擊控制項；點擊會以 Windows clipboard API 複製完整 remote path，Unicode build 使用 `CF_UNICODETEXT`，ANSI build 使用 `CF_TEXT`。滑鼠停在路徑上顯示手勢，clipboard 失敗才寫入 Output。
- 成功複製後會在路徑下方顯示 non-blocking native balloon tooltip；標題為 `Remote path copied to clipboard`，內容為完整 remote path。尖角對準顯示路徑文字的 80% 處；不使用系統 notification、不搶焦點，兩秒後自動消失，重複點擊會更新文字並重新計時。
- `git diff --check` 通過，`build.bat -Arch x64 -Config Release` 成功產出 DLL 與 ZIP；ZIP SHA-256：`1FA26C7DBA57243B35429636872750751589D695240618A1D2DF4CA8E78632D3`。尚待 Notepad++ 實機確認點擊後可貼上完整路徑與提示。

## 2026-08-17 Plan concurrent uploads and save priority

- 確定全域 `Concurrent uploads` 範圍為 1-8、預設 1，套用 FTP／FTPS／SFTP，於下次連線生效；不做 profile 個別覆寫。
- 確定只有 `NPPN_FILESAVED` 的 cache upload 提升為 urgent，僅插隊等待項目、不打斷已在傳輸的檔案；工具列 Abort 全部 active upload、queue 右鍵 Abort 僅作用於選取項目。
- 多國語系由使用者決定暫緩至後續工作，不與本輪併行 upload／save priority 混合開發。
- 使用者指定本輪 Global settings 對話框的固定字串改為正體中文；這是設定畫面的局部翻譯，不加入語言選擇或擴大為全 plugin i18n。
- SDD pre-flight 將 XML persistence 驗證拆成獨立 `ConcurrentUploadSettings` + TinyXML round-trip test，避免 unit test 連結 FTP cache、加密與 UI；排程 policy test 也改為先 promotion、再 dequeue，確保只檢查仍在等待中的項目。

## 2026-08-17 Concurrent upload scheduler foundation

- 新增 `UploadSchedulingPolicy`：normal lane 維持 FIFO、urgent lane 先取；等待中的 operation 可提升為 urgent，且可移除等待項目，不涉及 socket、thread 或 UI。
- 新增尚未接入 `FTPSession` 的 `ConcurrentUploadScheduler` 基礎：每個 worker 都從 prototype 建立獨立 `FTPClientWrapper::Clone()`，並重用既有單 worker `FTPQueue` 的 UI acknowledgement 與 per-worker progress monitor。operation 直到分派到空閒 worker 才綁定 wrapper。
- 關閉時會停止接收與分派、terminate notification wait、abort active clone、等待 dispatcher、deinitialize worker queue、disconnect/delete clone，最後取消並釋放尚未分派的 operation。
- focused policy test：`concurrent_upload_scheduler_exit=0`；`build.bat -Arch x64 -Config Release` 通過並產出 `_build\\Release\\NppFTP.dll` 與 `NppFTP-0.30.22-win64.zip`。此次 build 的既有 UTCP code page／legacy conversion warnings 未新增 error。
- 此 task 刻意不接入 session、設定、recursive batch 或 save priority；後續 task 才會進行真實 FTP／FTPS／SFTP worker 與 UI 實機驗證。

## 2026-08-17 Concurrent scheduler lifecycle correction

- scheduler 不再保存已交給 `FTPQueue` lifecycle 管理的 operation raw pointer；worker 是否忙碌改由既有 queue size 判定，shutdown 只 abort wrapper 並讓 `FTPQueue::Deinitialize()` 在自身 lock 內 terminate/delete operation，避免 polling 清除前的 use-after-free。
- 初始化從「先放開 state lock 再建立 worker」改為同一把 `Monitor` 全程保護，並加上 `m_initializing`；失敗時只清理尚未 publish 的 local worker list，不會遺失 queue thread 或 dispatcher handle。
- enqueue 新增受 lock 保護的 `PendingAdd` registry。duplicate 檢查、登記與 urgent promotion 皆在同一同步區段；只有 `QueueEventAdd` acknowledgement 結束後才原子放入可分派 policy，避免通知間隙的重複 enqueue 或 Start 早於 Add。
- shutdown 現在具 single-owner 行為：後續 `Deinitialize()` 會等第一個呼叫完成，只有第一個呼叫 wait/close dispatcher handle、回收 workers 與 pending operations。
- focused `concurrent_upload_scheduler_exit=0` 與 x64 Release build 均重新通過；實際 FTP／FTPS／SFTP 與 session integration 仍屬後續 Task 3 邊界。

## 2026-08-17 Concurrent scheduler pending-add queue count

- `ConcurrentUploadScheduler::GetQueueSize()` 現在包含尚在 `QueueEventAdd` acknowledgement 的 `m_pendingAdds`，避免 caller 在工作已被 scheduler 接手但尚未進入 dispatch lane 時誤判 queue 為 idle。
- 將三段 count 合併成純 header-only helper，focused test 明確驗證 waiting=0、pendingAdd=1、dispatched=0 時總數為 1；`concurrent_upload_scheduler_exit=0` 與 x64 Release build 均通過。

## 2026-08-17 Global concurrent upload settings

- 新增獨立 `ConcurrentUploadSettings`，統一 `maxConcurrentUploads` XML attribute 的讀寫與 1-8 範圍；缺值、非整數、零、負數或大於 8 一律回復為 1。
- `FTPSettings` 預設為 1，並透過同一 helper 載入、儲存與設定值正規化。Global settings 對話框新增「同時上傳數量」數字欄位與 1-8 spin control，仍在既有「確定」關閉流程儲存。
- 只將 `src/Windows/NppFTP.rc` 轉為 UTF-8 無 BOM，檔首加上 `#pragma code_page(65001)`，並僅把 Global settings caption、固定文字與確定按鈕改為正體中文；其他資源及 runtime 字串不在本 task 範圍。
- `NppFTP_FTPSettingsRoundTrip` 只連結 TinyXML 與 helper，確認 persistence boundary；focused test 與 x64 Release package build 均通過。真實 Notepad++ 對話框的顯示與 restart persistence 仍需實機 QA。

## 2026-08-17 Harden concurrent upload settings validation

- 修正原先 `QueryIntAttribute` 對帶尾碼數字的寬鬆解析；`maxConcurrentUploads` 現在逐字元確認完整整數，`abc`、`2.5`、`2abc`、空值與 overflow 都會回復為 1。這也修正前一筆 history 對「非整數已處理」的過度宣稱。
- focused round-trip test 改用明確 return code，不再依賴 Release build 會移除的 `assert`；並涵蓋三種 malformed attribute。
- Global settings 初始化時以 `UDM_SETBUDDY` 明確連結 1-8 spin control 與「同時上傳數量」edit，並用 `UDM_SETPOS32` 同步初始值，確保箭頭變更的是實際儲存欄位。
- x64 Release focused test 與 package build 均通過；`NppFTP.rc` 的 UTF-8 無 BOM 與 `#pragma code_page(65001)` 保持不變。真實 Notepad++ UI/restart QA 仍待執行。

## 2026-08-17 Route single-file uploads through concurrent workers

- `FTPSession` 在 main wrapper 完成 profile 與 certificate 設定後，依全域 1-8 設定建立 `ConcurrentUploadScheduler`；不等待 main connection 完成，各 clone worker 延用 `QueueUpload` 的既有 auto-connect 行為。single-file 手動上傳與 cache upload 進 scheduler，現階段全部保持 normal priority。
- download、remote browse、copy、mutation 與既有 recursive upload plan 仍留在序列 `m_transferQueue`／`m_mainQueue`；recursive batch 重整與 save urgent priority 明確保留給 Task 4、Task 5。
- session disconnect 提示會計入 scheduler 工作；Abort 會同時中止序列 transfer 與所有 active upload worker。queue 單筆取消可分辨 scheduler 未命中、等待中已取消及 active 不可取消，尚未重整的 recursive upload 仍能 fallback 到原 transfer queue。
- `Clear()` 與 StartSession failure 會先 deinitialize/delete scheduler，再釋放其 prototype wrapper。`QueueOperation::OnQueueTerminal()` 與 `FTPQueue` 的 ack 後 follow-up 基礎已接好，但本 task 沒有替 recursive upload 加 override。
- FTPWindow 改以 active transfer count 維持 busy/Abort 狀態，避免其中一個並行 worker 結束時誤把其他 active transfer 標成 idle。
- `NppFTP_ConcurrentUploadScheduler` Release focused test 與 `ftp_session_upload_routing.ps1` 均輸出 exit=0；`build.bat -Arch x64 -Config Release` 成功產出 `_build\\Release\\NppFTP.dll` 與 ZIP。既有 UTCP code-page／legacy conversion warnings 仍存在，未新增 build error。真實 FTP／FTPS／SFTP、Notepad++ task window、Abort/cancel 與 setting=1 相容性仍待實機 QA。

## 2026-08-17 Task 3 review fix round 1

- scheduler worker 現在從 dispatch adoption 到 `QueueEventRemove` 完成並退出 worker queue 之間持續登記其 operation；ownership 登記與 `FTPQueue` 接手在同一把 scheduler lock 內完成，取消、queue size 與 duplicate/conflict 檢查不再有 TakeNext 後的不可見空窗。worker 先清出完成項目再通知 scheduler，避免太早喚醒後沒有後續 signal 而停住。
- `QueueUpload::ConflictsWith` 以共同測試過的 local+remote path identity 判斷同一傳輸。若同檔已有 worker 持有，後續 normal upload 留在等待 lane；其他檔案仍可使用空閒 worker，同檔不會並行覆寫。save urgent/follow-up 規則仍未啟用。
- scheduler 接受 operation 時只送一次 `QueueEventAdd`；worker queue 以 `sendAddNotification=false` 接手，事件順序維持單一 Add、Start、End、Remove。
- FTPS clone 共用的 certificate vector 由 DLL 內同一把 recursive critical section 保護；certificate load、contains、accept 後二次檢查/append、settings save 與 free 均受鎖保護，避免多 worker auto-connect 同時修改 vector。lock state 由 thread-safe function static 建立並刻意維持到 DLL 卸載，不會早於全域 `NppFTP` destructor 失效，也不提高既有 Windows API target。
- `FTPQueue` 現在保存並檢查 `CreateThread` handle；建立失敗回傳 `-1`，成功 shutdown 會等待並關閉 handle，避免假成功、永久等待與每 session 最多八個 worker handle leak。
- focused Release tests 新增 blocked-conflict scheduling/path identity 與八執行緒 certificate lock；source contract 覆蓋 ownership 順序、單一 Add、thread handle lifecycle 與 certificate lock 接線。真實 FTP／FTPS／SFTP 同檔快速重複上傳、FTPS 首次憑證提示、取消 dispatch 中 operation 與關閉 session 仍待實機 QA。

## 2026-08-17 Parallel recursive upload batches

- recursive upload 改由單一 `QueueRemoteUploadPrepare` 留在序列 `m_transferQueue`，依 plan 的 parent-first directory view 建立／合併所有目錄；每個目錄失敗各自保留 path 與 failure kind，完成準備後才把 selected files 以 normal priority 交給 upload scheduler。
- `RemoteUploadBatch` 在 dispatch 前固定 selected／skipped 數量，success／failed／canceled／remaining 使用 `Interlocked` 更新；canceled paths 另以 critical section 保護。每個 `QueueRemoteUploadFile` 只會登記一次取消與一次 terminal，最後一個 terminal 才透過既有 `OnQueueTerminal()` 產生唯一 `QueueRemoteUploadComplete`。
- queued cancellation 現在會在 operation 刪除前取得 generic terminal follow-up；scheduler 仍不辨識 recursive operation 型別，且 ownership 在 terminal removal 前保持可見。active abort 仍沿用既有 per-operation failure 行為，不會清掉其他 worker 或等待中的檔案。
- summary Output 明確列出 selected、successful、failed、skipped、canceled 與 directory failures；file success/failure 仍在 UI thread 的 End acknowledgement 內記錄，completion marker 只能在所有 End acknowledgement 或 cancellation terminal 都完成後出現。
- Release-safe focused checks 覆蓋 parent-before-child、selected-only、skipped count、兩 worker 反向完成、exactly-once completion、failure/cancellation counters；`remote_upload_plan_exit=0`、`concurrent_upload_scheduler_exit=0`、`ftp_session_upload_routing_exit=0`。x64 Release DLL/package build 通過；真實 Notepad++ limit=2 directory drag/drop、distinct progress rows 與 FTP／FTPS／SFTP server 行為仍待實機 QA。

## 2026-08-18 Finalize recursive upload teardown lifecycle

- scheduler 關閉 worker 時改走 `FTPQueue::BeginTeardown()`；Start 已通知、仍在 500ms pre-execution Idle 的 upload 會先 terminal-cancel，斷線後不會再進入 `SendFile()`。Idle／Running 競態由 operation 內的 compare-exchange loop 收斂。
- `QueueOperation` 自行保存 execution state；`QueueUpload`、`QueueRemoteUploadPrepare` 與 `QueueRemoteUploadComplete` 在 `Perform()` 回傳前以 `CompletePerform()` 原子定案結果。Abort 只有在仍為 Running 時能取得取消權，已成功或已收到 server failure 的結果不會在返回空窗被改標 canceled。
- prepare 新增 teardown hook 與 files-dispatched one-shot：若目錄準備完成但 End 尚未由 UI 處理，confirmed teardown 會把尚未派送的 selected files 全數 terminal-account；若 UI 已交給 scheduler，則不會重複扣 remaining。session 已無法 dispatch 時也會立即取消未派送項目。
- `FTPQueue::Deinitialize()` 在 confirmed teardown 不再由 UI thread 補送 active End；operation 與 batch reference 仍完整回收，但 active `QueueRemoteUploadComplete` 不會在斷線期間漏出摘要。正常 session 路徑仍只顯示一次摘要。
- 真實 lifecycle 測試新增 scheduler Idle shutdown、兩 worker 反向完成、finished prepare before End、active completion marker teardown，以及 success/server-failure late Abort；`concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`ftp_session_upload_routing_exit=0`。
- `build.bat -Arch x64 -Config Release` 成功；DLL SHA-256 `A6EFD05F93CF4B8A2A82420A44A6287DAFE8BE2EF9B63307801DDF53C23BAB65`，ZIP SHA-256 `4A87450F60ED4FDCBFC65B32CD1EA4FF78BBA3927E55B4375F57B2A475125BEB`。既有 UTCP code-page／legacy conversion warnings 未新增 error；Notepad++ 加真實 FTP／FTPS／SFTP 的 disconnect/Abort 實機 QA 仍待執行。

## 2026-08-18 Close post-Start pre-Perform teardown handoff

- operation execution state 新增 `Starting`：queue 的 `StartExecution()` 只先選中工作，所有 production `Perform()` 必須在任何 connect／mkdir／transfer 等副作用前以 `BeginPerform()` 原子取得 `Running`。teardown 若先把 `Starting` 轉為 `Canceled`，worker 即使已走進原本選中的分支也只會返回，不會呼叫 SFTP `SendFile()` 或先以 `O_TRUNC` 打開遠端檔案。
- `BeginTeardown()` 只對已取得 `Running` 的 operation 呼叫 wrapper `Abort()`；尚在交接期的取消直接走 terminal accounting。`AbortActive()` 也會替被取消的 `Starting` operation 設定 canceled outcome，但不對尚未開始的 wrapper 發 Abort。
- lifecycle test 以真實 `FTPQueue` 與 `QueueRemoteUploadFile` 固定停在 Start 完成、production Perform 尚未開始的位置。修正前穩定失敗於 `handoff teardown never calls SendFile`；修正後確認 `SendFile=0`、wrapper Abort=0、canceled/remaining 正確，且 completion payload 只 post 一次。
- `concurrent_upload_scheduler_exit=0`、`remote_upload_plan_exit=0`、`ftp_queue_terminal_lifecycle_exit=0`、`ftp_session_upload_routing_exit=0`；`build.bat -Arch x64 -Config Release` 成功。DLL SHA-256 `738223775BE5F771BB1DCB3B7832F35FE097BF191E1EA3315C5A8373BB0EBA4E`，ZIP SHA-256 `458A6D164AC96003216F9D050A071A96F3D8B11672BB0414FB34E553AD1A34DD`。Task 5 urgent-save 行為未修改；真實 FTP／FTPS／SFTP teardown 實機 QA 仍待執行。

## 2026-08-18 Return remote browser to saved cache file

- `NPPN_FILESAVED` 的 cache upload 成功排入 urgent scheduler 後，會保留已解析的 remote file path，立即把 Remote Browser 導向該檔的父目錄；例如儲存 `/var/www/html/3waAIHub/index.php` 的 cache 後，畫面會切回 `/var/www/html/3waAIHub`。
- 目錄 listing 回來後會沿用既有 pending focus 機制選取剛儲存的檔案；若父目錄尚未在 object tree，完成 hierarchy listing 時仍會以 path 比對恢復焦點。使用者後續手動導覽會清除這個 pending focus，避免舊 save 結果搶回畫面。
- 新增 `remote_browser_saved_file_navigation` focused test，覆蓋巢狀路徑、根目錄檔案與無效路徑；routing contract 也確認只有 upload queue 接受成功才導向。全部 focused checks 與 `build.bat -Arch x64 -Config Release` 通過；尚待 Notepad++ 搭配真實 FTP／FTPS／SFTP 確認 save 後切換、refresh 與 list focus 體驗。
- 2026-08-19 已在 Notepad++ 搭配真實 FTP 驗證：從 `/var/www/html/3waAIHub/index.php` 切到其他目錄後儲存，Remote Browser 會切回檔案父目錄並保留 `index.php` 的選取與鍵盤 focus。
